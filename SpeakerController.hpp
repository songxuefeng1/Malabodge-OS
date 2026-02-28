#ifndef SPEAKERCONTROLLER_HPP
#define SPEAKERCONTROLLER_HPP

#include <windows.h>
#include <mmsystem.h>
#include <vector>
#include <stdexcept>
#include <mutex>
#include <memory>
#include <cmath>
#include <string>
#include <iostream>
#include <algorithm>
//Test Command: #error "lib include test acess."
// 自动链接winmm库，无需手动-lwinmm
#pragma comment(lib, "winmm.lib")

// 预定义M_PI（适配MinGW）
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class SpeakerController {
public:
    // 构造/析构（类内定义，默认inline）
    SpeakerController() noexcept = default;
    ~SpeakerController() { Cleanup(); }

    // 禁用拷贝，支持移动（类内定义，默认inline）
    SpeakerController(const SpeakerController&) = delete;
    SpeakerController& operator=(const SpeakerController&) = delete;
    SpeakerController(SpeakerController&& other) noexcept {
        std::lock_guard<std::mutex> lock(other.device_mutex_);
        wave_out_handle_ = other.wave_out_handle_;
        is_device_initialized_ = other.is_device_initialized_;
        other.wave_out_handle_ = nullptr;
        other.is_device_initialized_ = false;
    }
    SpeakerController& operator=(SpeakerController&& other) noexcept {
        if (this != &other) {
            std::lock_guard<std::mutex> self_lock(device_mutex_);
            std::lock_guard<std::mutex> other_lock(other.device_mutex_);
            Cleanup();
            wave_out_handle_ = other.wave_out_handle_;
            is_device_initialized_ = other.is_device_initialized_;
            other.wave_out_handle_ = nullptr;
            other.is_device_initialized_ = false;
        }
        return *this;
    }

    // 核心业务接口（类内定义，默认inline）
    void PlaySound(double frequency, double amplitude, DWORD duration) {
        ValidatePlayParameters(frequency, amplitude, duration);
        std::lock_guard<std::mutex> lock(device_mutex_);
        if (!is_device_initialized_) {
            InitializeAudioDevice();
        }

        std::unique_ptr<ManagedWaveData> managed_wave_data(new ManagedWaveData());
		managed_wave_data->audio_samples = GenerateSineWaveSamples(frequency, amplitude, duration);
        
        managed_wave_data->wave_header.lpData = reinterpret_cast<LPSTR>(managed_wave_data->audio_samples.data());
        managed_wave_data->wave_header.dwBufferLength = static_cast<DWORD>(managed_wave_data->audio_samples.size() * sizeof(short));
        managed_wave_data->wave_header.dwFlags = 0;

        MMRESULT prepare_result = waveOutPrepareHeader(wave_out_handle_, &managed_wave_data->wave_header, sizeof(WAVEHDR));
        if (prepare_result != MMSYSERR_NOERROR) {
            throw std::runtime_error("Failed to prepare wave header: " + GetWaveErrorString(prepare_result));
        }
        WaveHeaderGuard header_guard(wave_out_handle_, &managed_wave_data->wave_header);

        MMRESULT write_result = waveOutWrite(wave_out_handle_, &managed_wave_data->wave_header, sizeof(WAVEHDR));
        if (write_result != MMSYSERR_NOERROR) {
            throw std::runtime_error("Failed to write wave data: " + GetWaveErrorString(write_result));
        }

        while (waveOutUnprepareHeader(wave_out_handle_, &managed_wave_data->wave_header, sizeof(WAVEHDR)) == WAVERR_STILLPLAYING) {
            Sleep(10);
        }
    }

    // 静态函数（类内定义，默认inline）
    static void SimpleBeep(DWORD frequency, DWORD duration) {
        if (frequency < 37 || frequency > 32767) {
            throw std::invalid_argument(
                "Frequency for SimpleBeep must be between 37 and 32767 Hz (current: " + 
                std::to_string(frequency) + " Hz)"
            );
        }

#ifdef _WIN32
        BOOL beep_result = Beep(frequency, duration);
        if (!beep_result) {
            throw std::runtime_error(
                "SimpleBeep failed: System Beep API returned error - " + 
                GetSystemErrorString(GetLastError())
            );
        }
#else
        throw std::runtime_error("SimpleBeep is only supported on Windows operating system");
#endif
    }

    // 清理函数（类内定义，默认inline）
    void Cleanup() {
        std::lock_guard<std::mutex> lock(device_mutex_);
        if (wave_out_handle_) {
            waveOutReset(wave_out_handle_);
            MMRESULT close_result = waveOutClose(wave_out_handle_);
            if (close_result != MMSYSERR_NOERROR) {
                std::cerr << "[Warning] Failed to close audio device: " 
                          << GetWaveErrorString(close_result) << std::endl;
            }
            wave_out_handle_ = nullptr;
        }
        is_device_initialized_ = false;
    }

private:
    // 常量定义
    static constexpr int kSampleRate = 44100;
    static constexpr int kBitsPerSample = 16;
    static constexpr int kChannels = 1;
    static constexpr DWORD kMaxPlayDuration = 30000;
    static constexpr double kMinFrequency = 100.0;
    static constexpr double kMaxFrequency = 10000.0;
    static constexpr double kMinAmplitude = 0.0;
    static constexpr double kMaxAmplitude = 1.0;

    // 成员变量
    HWAVEOUT wave_out_handle_ = nullptr;
    bool is_device_initialized_ = false;
    std::mutex device_mutex_;

    // 嵌套RAII类
    class WaveHeaderGuard {
    public:
        WaveHeaderGuard(HWAVEOUT device_handle, WAVEHDR* wave_header)
            : device_handle_(device_handle), wave_header_(wave_header) {}
        
        // 析构函数显式inline（类内定义也可，这里显式标注更清晰）
        inline ~WaveHeaderGuard() noexcept {
            if (device_handle_ && wave_header_) {
                waveOutReset(device_handle_);
                waveOutUnprepareHeader(device_handle_, wave_header_, sizeof(WAVEHDR));
            }
        }

        WaveHeaderGuard(const WaveHeaderGuard&) = delete;
        WaveHeaderGuard& operator=(const WaveHeaderGuard&) = delete;

    private:
        HWAVEOUT device_handle_ = nullptr;
        WAVEHDR* wave_header_ = nullptr;
    };

    // 带生命周期管理的波形数据结构体
    struct ManagedWaveData {
        WAVEHDR wave_header{};
        std::vector<short> audio_samples;
    };

    // 私有工具函数（类内定义，默认inline）
    void InitializeAudioDevice() {
        WAVEFORMATEX audio_format{};
        audio_format.wFormatTag = WAVE_FORMAT_PCM;
        audio_format.nChannels = kChannels;
        audio_format.nSamplesPerSec = kSampleRate;
        audio_format.wBitsPerSample = kBitsPerSample;
        audio_format.nBlockAlign = (kChannels * kBitsPerSample) / 8;
        audio_format.nAvgBytesPerSec = kSampleRate * audio_format.nBlockAlign;

        MMRESULT open_result = waveOutOpen(
            &wave_out_handle_, 
            WAVE_MAPPER, 
            &audio_format, 
            0, 
            0, 
            CALLBACK_NULL
        );
        
        if (open_result != MMSYSERR_NOERROR) {
            throw std::runtime_error(
                "Failed to initialize audio device: " + GetWaveErrorString(open_result)
            );
        }

        is_device_initialized_ = true;
    }

    std::vector<short> GenerateSineWaveSamples(double frequency, double amplitude, DWORD duration) {
        const size_t sample_count = static_cast<size_t>(kSampleRate) * duration / 1000;
        std::vector<short> audio_samples(sample_count);

        const short max_amplitude = static_cast<short>(32767 * amplitude);
        for (size_t i = 0; i < sample_count; ++i) {
            const double time = static_cast<double>(i) / kSampleRate;
            const double angle = 2.0 * M_PI * frequency * time;
            audio_samples[i] = static_cast<short>(std::sin(angle) * max_amplitude);
        }

        return audio_samples;
    }

    void ValidatePlayParameters(double frequency, double amplitude, DWORD duration) {
        if (frequency < kMinFrequency || frequency > kMaxFrequency) {
            throw std::invalid_argument(
                "Frequency must be between " + std::to_string(kMinFrequency) + 
                " and " + std::to_string(kMaxFrequency) + " Hz (current: " + 
                std::to_string(frequency) + " Hz)"
            );
        }

        if (amplitude < kMinAmplitude || amplitude > kMaxAmplitude) {
            throw std::invalid_argument(
                "Amplitude must be between " + std::to_string(kMinAmplitude) + 
                " and " + std::to_string(kMaxAmplitude) + " (current: " + 
                std::to_string(amplitude) + ")"
            );
        }

        if (duration > kMaxPlayDuration) {
            throw std::invalid_argument(
                "Play duration cannot exceed " + std::to_string(kMaxPlayDuration) + 
                " ms (current: " + std::to_string(duration) + " ms)"
            );
        }
    }

    // 静态工具函数：显式inline static（关键！避免多重定义）
    inline static std::string GetWaveErrorString(MMRESULT error_code) {
        char error_buf[256] = {0};
        waveOutGetErrorText(error_code, error_buf, sizeof(error_buf));
        return std::string(error_buf) + " (error code: " + std::to_string(error_code) + ")";
    }

    inline static std::string GetSystemErrorString(DWORD error_code) {
        char error_buf[256] = {0};
        FormatMessage(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            error_code,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            error_buf,
            sizeof(error_buf),
            nullptr
        );
        return std::string(error_buf) + " (error code: " + std::to_string(error_code) + ")";
    }
};

#endif // SPEAKERCONTROLLER_HPP