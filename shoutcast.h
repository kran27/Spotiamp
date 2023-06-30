#ifndef TINY_SPOTIFY_SHOUTCAST_H_
#define TINY_SPOTIFY_SHOUTCAST_H_

#include "types.h"
#include "tiny_spotify.h"

typedef struct Shoutcast Shoutcast;

// Create an instance of the shoutcast streamer
Shoutcast *ShoutcastCreate(Tsp *tsp);

// Destroy the shoutcast streamer
void ShoutcastDestroy(Shoutcast *shoutcast);

// Start listening on a port for incoming shoutcast connections
bool ShoutcastListen(Shoutcast *shoutcast, int port);

// Push bytes to the shoutcast streamer. This function will not accept
// bytes unless there's at least one connection.
int ShoutcastWavPush(void *context, int flags,
                     const TspSampleType *datai, int sizei,
                     const TspSampleFormat *sample_format, int *samples_buffered);

// Set the name of the now playing track
void ShoutcastSetNowPlaying(Shoutcast *shoutcast, TspItem *item);

// Return the number of connected clients
int ShoutcastNumClients(Shoutcast *shoutcast);


#endif  // TINY_SPOTIFY_SHOUTCAST_H_
