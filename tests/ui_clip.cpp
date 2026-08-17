#include <game/client/ui.h>

#include <cstdio>

static bool Expect(bool Condition, const char *pMessage)
{
	if(!Condition)
		std::fprintf(stderr, "ui clip: %s\n", pMessage);
	return Condition;
}

int main()
{
	CUIRect Clip = {0.0f, 100.0f, 200.0f, 200.0f};
	CUIRect Card = {10.0f, 20.0f, 180.0f, 90.0f};
	bool Ok = true;
	Ok &= Expect(UiMouseInsideClipped(50.0f, 50.0f, &Card, 0) == 1, "unclipped hits card");
	Ok &= Expect(UiMouseInsideClipped(50.0f, 50.0f, &Card, &Clip) == 0, "card above clip is not hittable");
	Ok &= Expect(UiMouseInsideClipped(50.0f, 105.0f, &Card, &Clip) == 1, "overlap with clip remains hittable");
	Ok &= Expect(UiMouseInsideClipped(50.0f, 150.0f, &Card, &Clip) == 0, "below card is not hittable");
	return Ok ? 0 : 1;
}
