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

		struct TriangleControlValues {
			int period = 1024;
			int linearPeriod = 64;
			int length = -1;
		};

		TriangleControlValues triangle;

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
				/*PlayPulse(pAudioContext,
					idx, 
					pulses[idx].duty, 
					//pulses[idx].volume, 
					pulses[idx].period, 
					pulses[idx].sweepEnabled,
					pulses[idx].sweepAmount,
					pulses[idx].sweepPeriod,
					pulses[idx].sweepNegate,
					//!pulses[idx].useEnvelope,
					pulses[idx].length);*/

				bool loop = pulses[idx].length == -1;
				u8 lengthCounterLoad = loop ? 0 : pulses[idx].length;

				u8 byte0 = (pulses[idx].duty << 6) | (loop << 5) | (!pulses[idx].useEnvelope << 4) | pulses[idx].volume;
				u8 byte1 = (pulses[idx].sweepEnabled << 7) | (pulses[idx].sweepPeriod << 4) | (pulses[idx].sweepNegate << 3) | pulses[idx].sweepAmount;
				u8 byte2 = pulses[idx].period & 0xff;
				u8 byte3 = (lengthCounterLoad << 3) | (pulses[idx].period >> 8);

				WritePulse(pAudioContext, idx, 0, byte0);
				WritePulse(pAudioContext, idx, 1, byte1);
				WritePulse(pAudioContext, idx, 2, byte2);
				WritePulse(pAudioContext, idx, 3, byte3);
			}

			ImGui::PopID();
		}

		void DrawAudioWindow(AudioContext* pAudioContext) {
			ImGui::Begin("Audio");

			ReadDebugBuffer(pAudioContext, buffer, 1024);
			ImGui::PlotLines("Waveform", GetSample, buffer, 1024, 0, nullptr, 0.0f, 255.0f, ImVec2(0, 80.0f));

			DrawPulseControls(pEditorContext, pAudioContext, 0);
			DrawPulseControls(pEditorContext, pAudioContext, 1);

			ImGui::Text("Triangle");
			ImGui::SliderInt("Period", &triangle.period, 0, 0x7ff);
			ImGui::SliderInt("Linear period", &triangle.linearPeriod, 0, 0x7f);
			ImGui::SliderInt("Length (index)", &triangle.length, -1, 31);

			if (ImGui::Button("Play Triangle")) {
				/*PlayTriangle(pAudioContext,
					triangle.period,
					triangle.linearPeriod);*/

				bool loop = triangle.length == -1;
				u8 lengthCounterLoad = loop ? 0 : triangle.length;

				u8 byte0 = (loop << 7) | triangle.linearPeriod;
				u8 byte1 = 0;
				u8 byte2 = triangle.period & 0xff;
				u8 byte3 = (lengthCounterLoad << 3) | (triangle.period >> 8);

				WriteTriangle(pAudioContext, 0, byte0);
				WriteTriangle(pAudioContext, 1, byte1);
				WriteTriangle(pAudioContext, 2, byte2);
				WriteTriangle(pAudioContext, 3, byte3);
			}

			ImGui::End();
		}
	}
}