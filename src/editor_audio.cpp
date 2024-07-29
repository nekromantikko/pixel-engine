#include "editor_audio.h"
#include "imgui.h"

namespace Editor {
	namespace Audio {
		u8 buffer[1024];

		r32 GetSample(void* data, s32 idx) {
			return ((r32)((u8*)data)[idx]) - 128.f;
		}

		void DrawAudioWindow(AudioContext* pAudioContext) {
			ImGui::Begin("Audio");

			PeekAudioBuffer(pAudioContext, buffer, 1024);
			ImGui::PlotLines("Waveform", GetSample, buffer, 1024, 0, nullptr, -128.0f, 128.0f, ImVec2(0, 80.0f));

			ImGui::End();
		}
	}
}