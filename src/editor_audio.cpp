#include "editor_audio.h"
#include "imgui.h"

namespace Editor {
	namespace Audio {
		u8 buffer[1024];

		struct PulseControlValues {
			int duty = 0;
			int volume = 3;
			int period = 1024;

			bool sweepEnabled = false;
			int sweepPeriod = 3;
			int sweepAmount = 3;
			bool sweepNegate = false;

			bool useEnvelope = false;
			int length = -1;
		};

		PulseControlValues pulses[2];

		static r32 GetSample(void* data, s32 idx) {
			return ((r32)((u8*)data)[idx]);
		}

		static void DrawPulseControls(EditorContext* pEditorContext, AudioContext* pAudioContext, bool idx) {
			ImGui::Text("Pulse %d", idx == 0 ? 1 : 2);

			ImGui::PushID(idx);

			ImGui::SliderInt("Duty cycle", &pulses[idx].duty, 0, 3);

			ImGui::SliderInt("Volume", &pulses[idx].volume, 0, 15);

			ImGui::SliderInt("Period", &pulses[idx].period, 0, 0x7ff);

			ImGui::Checkbox("Enable sweep", &pulses[idx].sweepEnabled);
			ImGui::SliderInt("Sweep period", &pulses[idx].sweepPeriod, 0, 7);
			ImGui::SliderInt("Sweep amount", &pulses[idx].sweepAmount, 0, 7);
			ImGui::Checkbox("Negate sweep", &pulses[idx].sweepNegate);

			ImGui::Checkbox("Use envelope", &pulses[idx].useEnvelope);

			ImGui::SliderInt("Length (index)", &pulses[idx].length, -1, 31);
			
			if (ImGui::Button("Play")) {
				PlayPulse(pAudioContext, 
					idx, 
					pulses[idx].duty, 
					pulses[idx].volume, 
					pulses[idx].period, 
					pulses[idx].sweepEnabled,
					pulses[idx].sweepAmount,
					pulses[idx].sweepPeriod,
					pulses[idx].sweepNegate,
					!pulses[idx].useEnvelope,
					pulses[idx].length);
			}

			ImGui::PopID();
		}

		void DrawAudioWindow(AudioContext* pAudioContext) {
			ImGui::Begin("Audio");

			ReadDebugBuffer(pAudioContext, buffer, 1024);
			ImGui::PlotLines("Waveform", GetSample, buffer, 1024, 0, nullptr, 0.0f, 255.0f, ImVec2(0, 80.0f));

			DrawPulseControls(pEditorContext, pAudioContext, 0);
			DrawPulseControls(pEditorContext, pAudioContext, 1);

			ImGui::End();
		}
	}
}