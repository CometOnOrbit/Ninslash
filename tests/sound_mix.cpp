#include <base/audio_math.h>

#include <cassert>

static int FillLoopTicks(int Tick, int NumFrames, int Want, int Loop, int *pOut)
{
	int Written = 0;
	int t = Tick;
	while(Written < Want)
	{
		int Span = SoundMixSpan(t, NumFrames, Want - Written);
		if(Span <= 0)
		{
			if(Loop && NumFrames > 0 && t != 0)
			{
				t = 0;
				continue;
			}
			break;
		}
		for(int i = 0; i < Span; i++)
			pOut[Written++] = t++;
		if(t >= NumFrames && Loop)
			t = 0;
	}
	return Written;
}

int main()
{
	assert(SoundMixSpan(0, 100, 32) == 32);
	assert(SoundMixSpan(90, 100, 32) == 10);
	assert(SoundMixSpan(100, 100, 32) == 0);
	assert(SoundMixSpan(0, 0, 32) == 0);
	assert(SoundMixSpan(-1, 100, 32) == 0);

	int aTicks[32];
	assert(FillLoopTicks(90, 100, 32, 1, aTicks) == 32);
	for(int i = 0; i < 10; i++)
		assert(aTicks[i] == 90 + i);
	for(int i = 10; i < 32; i++)
		assert(aTicks[i] == i - 10);

	assert(FillLoopTicks(90, 100, 32, 0, aTicks) == 10);
	for(int i = 0; i < 10; i++)
		assert(aTicks[i] == 90 + i);

	assert(SoundMusicVolIndex(0) == 0);
	assert(SoundMusicVolIndex(SOUND_MUSIC_VOL_RAMP - 1) == SOUND_MUSIC_VOL_RAMP - 1);
	assert(SoundMusicVolIndex(SOUND_MUSIC_VOL_RAMP) == SOUND_MUSIC_VOL_RAMP);
	assert(SoundMusicVolIndex(1024) == SOUND_MUSIC_VOL_RAMP);
	return 0;
}
