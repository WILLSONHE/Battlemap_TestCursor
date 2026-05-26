#include "BattleWindowsAudioInput.h"

#if PLATFORM_WINDOWS
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "mmdevapi.lib")
#pragma comment(lib, "propsys.lib")
#pragma comment(lib, "uuid.lib")
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propsys.h>
#include <ksmedia.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#if PLATFORM_WINDOWS
namespace
{
	struct FComScope
	{
		bool bNeedsUninit = false;
		FComScope()
		{
			const HRESULT Hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
			bNeedsUninit = (Hr == S_OK);
		}
		~FComScope()
		{
			if (bNeedsUninit)
			{
				CoUninitialize();
			}
		}
	};

	IMMDevice* ResolveCaptureDevice(const FString& DeviceId, FString* OutError)
	{
		IMMDeviceEnumerator* Enumerator = nullptr;
		HRESULT Hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&Enumerator));
		if (FAILED(Hr) || !Enumerator)
		{
			if (OutError)
			{
				*OutError = FString::Printf(TEXT("MMDeviceEnumerator failed (0x%08X)."), static_cast<uint32>(Hr));
			}
			return nullptr;
		}

		IMMDevice* Device = nullptr;
		if (DeviceId.IsEmpty())
		{
			Hr = Enumerator->GetDefaultAudioEndpoint(eCapture, eCommunications, &Device);
			if (FAILED(Hr) || !Device)
			{
				Hr = Enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &Device);
			}
		}
		else
		{
			Hr = Enumerator->GetDevice(TCHAR_TO_WCHAR(*DeviceId), &Device);
		}
		Enumerator->Release();

		if (FAILED(Hr) || !Device)
		{
			if (OutError)
			{
				*OutError = FString::Printf(TEXT("Capture device not found (0x%08X)."), static_cast<uint32>(Hr));
			}
			return nullptr;
		}
		return Device;
	}

	bool IsFloatPcmFormat(const WAVEFORMATEX* Format)
	{
		if (!Format)
		{
			return false;
		}
		if (Format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
		{
			return true;
		}
		if (Format->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
		{
			const WAVEFORMATEXTENSIBLE* Ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(Format);
			return Ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
		}
		return false;
	}

	float ComputePeakFromBuffer(const BYTE* Data, const UINT32 NumFrames, const WAVEFORMATEX* Format)
	{
		if (!Data || !Format || NumFrames == 0 || Format->nChannels == 0)
		{
			return 0.f;
		}

		float Peak = 0.f;
		const int32 Channels = static_cast<int32>(Format->nChannels);

		if (IsFloatPcmFormat(Format))
		{
			const float* Samples = reinterpret_cast<const float*>(Data);
			const int32 SampleCount = static_cast<int32>(NumFrames) * Channels;
			for (int32 i = 0; i < SampleCount; ++i)
			{
				Peak = FMath::Max(Peak, FMath::Abs(Samples[i]));
			}
		}
		else if (Format->wFormatTag == WAVE_FORMAT_PCM && Format->wBitsPerSample == 16)
		{
			const int16* Samples = reinterpret_cast<const int16*>(Data);
			const int32 SampleCount = static_cast<int32>(NumFrames) * Channels;
			for (int32 i = 0; i < SampleCount; ++i)
			{
				Peak = FMath::Max(Peak, FMath::Abs(static_cast<float>(Samples[i]) / 32768.f));
			}
		}
		else if (Format->wFormatTag == WAVE_FORMAT_PCM && Format->wBitsPerSample == 32)
		{
			const int32* Samples = reinterpret_cast<const int32*>(Data);
			const int32 SampleCount = static_cast<int32>(NumFrames) * Channels;
			for (int32 i = 0; i < SampleCount; ++i)
			{
				Peak = FMath::Max(Peak, FMath::Abs(static_cast<float>(Samples[i]) / 2147483648.f));
			}
		}

		return FMath::Clamp(Peak, 0.f, 1.f);
	}

	struct FCaptureMeterSession
	{
		IAudioClient* AudioClient = nullptr;
		IAudioCaptureClient* CaptureClient = nullptr;
		WAVEFORMATEX* MixFormat = nullptr;
		FString ActiveDeviceId;
		bool bRunning = false;

		void Shutdown()
		{
			if (CaptureClient)
			{
				CaptureClient->Release();
				CaptureClient = nullptr;
			}
			if (AudioClient)
			{
				if (bRunning)
				{
					AudioClient->Stop();
				}
				AudioClient->Release();
				AudioClient = nullptr;
			}
			if (MixFormat)
			{
				CoTaskMemFree(MixFormat);
				MixFormat = nullptr;
			}
			ActiveDeviceId.Reset();
			bRunning = false;
		}

		bool Start(const FString& DeviceId, FString* OutError)
		{
			Shutdown();

			IMMDevice* Device = ResolveCaptureDevice(DeviceId, OutError);
			if (!Device)
			{
				return false;
			}

			HRESULT Hr = Device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&AudioClient));
			Device->Release();
			if (FAILED(Hr) || !AudioClient)
			{
				if (OutError)
				{
					*OutError = FString::Printf(TEXT("IAudioClient failed (0x%08X)."), static_cast<uint32>(Hr));
				}
				Shutdown();
				return false;
			}

			Hr = AudioClient->GetMixFormat(&MixFormat);
			if (FAILED(Hr) || !MixFormat)
			{
				if (OutError)
				{
					*OutError = FString::Printf(TEXT("GetMixFormat failed (0x%08X)."), static_cast<uint32>(Hr));
				}
				Shutdown();
				return false;
			}

			const REFERENCE_TIME BufferDuration = 1000000; // 100 ms
			Hr = AudioClient->Initialize(
				AUDCLNT_SHAREMODE_SHARED,
				0,
				BufferDuration,
				0,
				MixFormat,
				nullptr);
			if (FAILED(Hr))
			{
				if (OutError)
				{
					*OutError = FString::Printf(TEXT("AudioClient Initialize failed (0x%08X)."), static_cast<uint32>(Hr));
				}
				Shutdown();
				return false;
			}

			Hr = AudioClient->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void**>(&CaptureClient));
			if (FAILED(Hr) || !CaptureClient)
			{
				if (OutError)
				{
					*OutError = FString::Printf(TEXT("IAudioCaptureClient failed (0x%08X)."), static_cast<uint32>(Hr));
				}
				Shutdown();
				return false;
			}

			Hr = AudioClient->Start();
			if (FAILED(Hr))
			{
				if (OutError)
				{
					*OutError = FString::Printf(TEXT("AudioClient Start failed (0x%08X)."), static_cast<uint32>(Hr));
				}
				Shutdown();
				return false;
			}

			ActiveDeviceId = DeviceId;
			bRunning = true;
			return true;
		}

		float Poll(FString* OutError)
		{
			if (!bRunning || !CaptureClient || !MixFormat)
			{
				return 0.f;
			}

			float Peak = 0.f;
			UINT32 PacketLength = 0;
			while (true)
			{
				HRESULT Hr = CaptureClient->GetNextPacketSize(&PacketLength);
				if (FAILED(Hr))
				{
					if (OutError)
					{
						*OutError = FString::Printf(TEXT("GetNextPacketSize failed (0x%08X)."), static_cast<uint32>(Hr));
					}
					break;
				}
				if (PacketLength == 0)
				{
					break;
				}

				BYTE* Data = nullptr;
				UINT32 NumFrames = 0;
				DWORD Flags = 0;
				UINT64 DevicePosition = 0;
				UINT64 QpcPosition = 0;
				Hr = CaptureClient->GetBuffer(&Data, &NumFrames, &Flags, &DevicePosition, &QpcPosition);
				if (FAILED(Hr))
				{
					if (OutError)
					{
						*OutError = FString::Printf(TEXT("GetBuffer failed (0x%08X)."), static_cast<uint32>(Hr));
					}
					break;
				}

				if (!(Flags & AUDCLNT_BUFFERFLAGS_SILENT) && Data && NumFrames > 0)
				{
					Peak = FMath::Max(Peak, ComputePeakFromBuffer(Data, NumFrames, MixFormat));
				}

				CaptureClient->ReleaseBuffer(NumFrames);
			}

			return Peak;
		}
	};

	FCaptureMeterSession GCaptureMeter;
}
#endif

bool FBattleWindowsAudioInput::EnumerateCaptureDevices(TArray<FBattleAudioInputDeviceInfo>& OutDevices, FString* OutError)
{
	OutDevices.Reset();
#if PLATFORM_WINDOWS
	FComScope Com;
	IMMDeviceEnumerator* Enumerator = nullptr;
	HRESULT Hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&Enumerator));
	if (FAILED(Hr) || !Enumerator)
	{
		if (OutError)
		{
			*OutError = FString::Printf(TEXT("MMDeviceEnumerator failed (0x%08X)."), static_cast<uint32>(Hr));
		}
		return false;
	}

	IMMDeviceCollection* Collection = nullptr;
	Hr = Enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &Collection);
	Enumerator->Release();
	if (FAILED(Hr) || !Collection)
	{
		if (OutError)
		{
			*OutError = FString::Printf(TEXT("EnumAudioEndpoints failed (0x%08X)."), static_cast<uint32>(Hr));
		}
		return false;
	}

	UINT Count = 0;
	Collection->GetCount(&Count);
	for (UINT i = 0; i < Count; ++i)
	{
		IMMDevice* Device = nullptr;
		if (FAILED(Collection->Item(i, &Device)) || !Device)
		{
			continue;
		}

		LPWSTR EndpointId = nullptr;
		if (FAILED(Device->GetId(&EndpointId)) || !EndpointId)
		{
			Device->Release();
			continue;
		}

		FString DisplayName = FString(EndpointId);
		IPropertyStore* Props = nullptr;
		if (SUCCEEDED(Device->OpenPropertyStore(STGM_READ, &Props)) && Props)
		{
			PROPVARIANT NameVar;
			PropVariantInit(&NameVar);
			if (SUCCEEDED(Props->GetValue(PKEY_Device_FriendlyName, &NameVar)) && NameVar.vt == VT_LPWSTR && NameVar.pwszVal)
			{
				DisplayName = FString(NameVar.pwszVal);
			}
			PropVariantClear(&NameVar);
			Props->Release();
		}

		FBattleAudioInputDeviceInfo Info;
		Info.DeviceId = FString(EndpointId);
		Info.DisplayName = DisplayName;
		OutDevices.Add(Info);
		CoTaskMemFree(EndpointId);
		Device->Release();
	}
	Collection->Release();
	return OutDevices.Num() > 0;
#else
	if (OutError)
	{
		*OutError = TEXT("Audio capture devices are only supported on Windows.");
	}
	return false;
#endif
}

float FBattleWindowsAudioInput::QueryCapturePeakLevel(const FString& DeviceId, FString* OutError)
{
	if (!IsCaptureMeterActive())
	{
		StartCaptureMeter(DeviceId, OutError);
	}
	return PollCaptureMeterPeak(OutError);
}

bool FBattleWindowsAudioInput::StartCaptureMeter(const FString& DeviceId, FString* OutError)
{
#if PLATFORM_WINDOWS
	FComScope Com;
	return GCaptureMeter.Start(DeviceId, OutError);
#else
	if (OutError)
	{
		*OutError = TEXT("Capture metering is only supported on Windows.");
	}
	return false;
#endif
}

void FBattleWindowsAudioInput::StopCaptureMeter()
{
#if PLATFORM_WINDOWS
	GCaptureMeter.Shutdown();
#endif
}

bool FBattleWindowsAudioInput::IsCaptureMeterActive()
{
#if PLATFORM_WINDOWS
	return GCaptureMeter.bRunning;
#else
	return false;
#endif
}

float FBattleWindowsAudioInput::PollCaptureMeterPeak(FString* OutError)
{
#if PLATFORM_WINDOWS
	if (!GCaptureMeter.bRunning)
	{
		return 0.f;
	}
	return GCaptureMeter.Poll(OutError);
#else
	if (OutError)
	{
		*OutError = TEXT("Capture metering is only supported on Windows.");
	}
	return 0.f;
#endif
}
