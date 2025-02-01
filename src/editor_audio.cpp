#include "editor_audio.h"
#include "imgui.h"

namespace Editor {
	namespace Audio {
		u8 buffer[1024];

		static r32 GetSample(void* data, s32 idx) {
			return ((r32)((u8*)data)[idx]);
		}

		static void DrawPulseControls(bool idx) {
			ImGui::SeparatorText(idx == 0 ? "Pulse 1" : "Pulse 2");
			const u32 channel = idx == 0 ? CHAN_ID_PULSE0 : CHAN_ID_PULSE1;

			ImGui::PushID(idx);

			u8 bytes[4]{};
			ReadChannel(channel, bytes);

			int duty = bytes[0] >> 6;
			int volume = bytes[0] & 0b00001111;
			int period = bytes[2] | ((bytes[3] & 0x07) << 8);
			bool sweepEnabled = (bytes[1] >> 7);
			int sweepPeriod = (bytes[1] >> 4) & 0x07;
			int sweepAmount = (bytes[1] & 0x07);
			bool sweepNegate = (bytes[1] >> 3) & 1;
			bool useEnvelope = !((bytes[0] >> 4) & 1);
			bool loop = (bytes[0] >> 5) & 1;
			int lengthCounterLoad = (bytes[3] >> 3);

			if (ImGui::SliderInt("Duty cycle", &duty, 0, 3)) {
				u8 byte0 = (duty << 6) | (loop << 5) | (!useEnvelope << 4) | volume;
				WriteChannel(channel, 0, byte0);
			}

			if (ImGui::SliderInt("Volume", &volume, 0, 15)) {
				u8 byte0 = (duty << 6) | (loop << 5) | (!useEnvelope << 4) | volume;
				WriteChannel(channel, 0, byte0);
			}

			if (ImGui::SliderInt("Period", &period, 0, 0x7ff)) {
				u8 byte2 = period & 0xff;
				u8 byte3 = (lengthCounterLoad << 3) | (period >> 8);
				WriteChannel(channel, 2, byte2);
				WriteChannel(channel, 3, byte3);
			}

			if (ImGui::Checkbox("Enable sweep", &sweepEnabled)) {
				u8 byte1 = (sweepEnabled << 7) | (sweepPeriod << 4) | (sweepNegate << 3) | sweepAmount;
				WriteChannel(channel, 1, byte1);
			}
			if (ImGui::SliderInt("Sweep period", &sweepPeriod, 0, 7)) {
				u8 byte1 = (sweepEnabled << 7) | (sweepPeriod << 4) | (sweepNegate << 3) | sweepAmount;
				WriteChannel(channel, 1, byte1);
			}
			if (ImGui::SliderInt("Sweep amount", &sweepAmount, 0, 7)) {
				u8 byte1 = (sweepEnabled << 7) | (sweepPeriod << 4) | (sweepNegate << 3) | sweepAmount;
				WriteChannel(channel, 1, byte1);
			}
			if (ImGui::Checkbox("Negate sweep", &sweepNegate)) {
				u8 byte1 = (sweepEnabled << 7) | (sweepPeriod << 4) | (sweepNegate << 3) | sweepAmount;
				WriteChannel(channel, 1, byte1);
			}

			if (ImGui::Checkbox("Use envelope", &useEnvelope)) {
				u8 byte0 = (duty << 6) | (loop << 5) | (!useEnvelope << 4) | volume;
				WriteChannel(channel, 0, byte0);
			}

			if (ImGui::Checkbox("Loop", &loop)) {
				u8 byte0 = (duty << 6) | (loop << 5) | (!useEnvelope << 4) | volume;
				WriteChannel(channel, 0, byte0);
			}

			if (ImGui::SliderInt("Length counter load", &lengthCounterLoad, 0, 31)) {
				u8 byte3 = (lengthCounterLoad << 3) | (period >> 8);
				WriteChannel(channel, 3, byte3);
			}

			ImGui::PopID();
		}

		static void DrawTriangleControls() {
			ImGui::SeparatorText("Triangle");

			ImGui::PushID(2);

			u8 bytes[4]{};
			ReadChannel(CHAN_ID_TRIANGLE, bytes);

			int period = bytes[2] | ((bytes[3] & 0x07) << 8);
			int linearPeriod = bytes[0] & 0x7f;
			bool loop = bytes[0] >> 7;
			int lengthCounterLoad = (bytes[3] >> 3);

			if (ImGui::SliderInt("Period", &period, 0, 0x7ff)) {
				u8 byte2 = period & 0xff;
				u8 byte3 = (lengthCounterLoad << 3) | (period >> 8);
				WriteChannel(CHAN_ID_TRIANGLE, 2, byte2);
				WriteChannel(CHAN_ID_TRIANGLE, 3, byte3);
			}
			if (ImGui::SliderInt("Linear period", &linearPeriod, 0, 0x7f)) {
				u8 byte0 = (loop << 7) | linearPeriod;
				WriteChannel(CHAN_ID_TRIANGLE, 0, byte0);
			}

			if (ImGui::Checkbox("Loop", &loop)) {
				u8 byte0 = (loop << 7) | linearPeriod;
				WriteChannel(CHAN_ID_TRIANGLE, 0, byte0);
			}

			if (ImGui::SliderInt("Length counter load", &lengthCounterLoad, 0, 31)) {
				u8 byte3 = (lengthCounterLoad << 3) | (period >> 8);
				WriteChannel(CHAN_ID_TRIANGLE, 3, byte3);
			}

			ImGui::PopID();
		}

		static void DrawNoiseControls() {
			ImGui::SeparatorText("Noise");

			ImGui::PushID(3);

			u8 bytes[4]{};
			ReadChannel(CHAN_ID_NOISE, bytes);

			int volume = bytes[0] & 0b00001111;
			bool useEnvelope = !((bytes[0] >> 4) & 1);
			bool loop = (bytes[0] >> 5) & 1;
			int period = bytes[2] & 0x0f;
			bool mode = bytes[2] >> 7;
			int lengthCounterLoad = (bytes[3] >> 3);

			if (ImGui::Checkbox("Mode", &mode)) {
				u8 byte2 = (mode << 7) | period;
				WriteChannel(CHAN_ID_NOISE, 2, byte2);
			}

			if (ImGui::SliderInt("Volume", &volume, 0, 15)) {
				u8 byte0 = (loop << 5) | (!useEnvelope << 4) | volume;
				WriteChannel(CHAN_ID_NOISE, 0, byte0);
			}

			if (ImGui::SliderInt("Period", &period, 0, 0x0f)) {
				u8 byte2 = (mode << 7) | period;
				WriteChannel(CHAN_ID_NOISE, 2, byte2);
			}

			if (ImGui::Checkbox("Use envelope", &useEnvelope)) {
				u8 byte0 = (loop << 5) | (!useEnvelope << 4) | volume;
				WriteChannel(CHAN_ID_NOISE, 0, byte0);
			}

			if (ImGui::Checkbox("Loop", &loop)) {
				u8 byte0 = (loop << 5) | (!useEnvelope << 4) | volume;
				WriteChannel(CHAN_ID_NOISE, 0, byte0);
			}

			if (ImGui::SliderInt("Length counter load", &lengthCounterLoad, 0, 31)) {
				u8 byte3 = (lengthCounterLoad << 3);
				WriteChannel(CHAN_ID_NOISE, 3, byte3);
			}

			ImGui::PopID();
		}

		void DrawAudioWindow() {
			ImGui::Begin("Audio");

			ReadDebugBuffer(buffer, 1024);
			ImGui::PlotLines("Waveform", GetSample, buffer, 1024, 0, nullptr, 0.0f, 255.0f, ImVec2(0, 80.0f));

			DrawPulseControls(0);
			DrawPulseControls(1);
			DrawTriangleControls();
			DrawNoiseControls();

			ImGui::End();
		}
	}
}