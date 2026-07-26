#ifndef ENGINE_PLATFORM_TRANSPORT_H
#define ENGINE_PLATFORM_TRANSPORT_H

/*
 * A transport is deliberately below the game protocol. UDP remains the
 * default implementation; a Steam NetworkingSockets implementation may use
 * the same reliable chunks and snapshots without duplicating game code.
 */
class IPlatformTransport
{
public:
	enum EKind
	{
		KIND_UDP,
		KIND_STEAM_RELAY,
	};

	virtual ~IPlatformTransport() {}
	virtual EKind Kind() const = 0;
	virtual bool IsAvailable() const = 0;
	virtual const char *Name() const = 0;
};

#endif
