#ifndef TINY_SPOTIFY_H_
#define TINY_SPOTIFY_H_

#include <stddef.h>

// MSVC does not have the stdint.h header
#if !defined(_MSC_VER)
#include <stdint.h>
#endif

// If obfuscation is enabled, bring in obfuscated names.
#if TSP_OBFUSCATE
#include "tiny_spotify_obfuscate.h"
#endif

#ifndef TSP_PUBLIC
#define TSP_PUBLIC
#endif

#ifndef TSP_PRIVATE
#define TSP_PRIVATE
#endif

// Declarations of Tsp types.
typedef unsigned char TspBool;

#if defined(_MSC_VER)
typedef signed __int16 TspInt16;
typedef unsigned __int32 TspUint32;
typedef signed __int32 TspInt32;
typedef unsigned __int64 TspUint64;
typedef signed __int64 TspInt64;
#else
typedef int16_t TspInt16;
typedef uint32_t TspUint32;
typedef int32_t TspInt32;
typedef uint64_t TspUint64;
typedef int64_t TspInt64;
#endif

#ifndef TSP_SAMPLE_TYPE_DEFINED
#define TSP_SAMPLE_TYPE_DEFINED
#if TSP_USE_SAMPLE_TYPE_INT32
typedef TspInt32 TspSampleType;
#else  // TSP_USE_SAMPLE_TYPE_INT32
typedef TspInt16 TspSampleType;
#endif  // TSP_USE_SAMPLE_TYPE_INT32
#endif  // TSP_SAMPLE_TYPE_DEFINED

typedef struct Tsp Tsp;
typedef struct TspItem TspItem;
typedef struct TspItemList TspItemList;
typedef struct TspAllocator TspAllocator;
typedef struct TspMemoryLimits TspMemoryLimits;
typedef struct TspSocketManager TspSocketManager;
typedef struct TspTrackFileReader TspTrackFileReader;
typedef struct TspAudioStreamer TspAudioStreamer;
typedef struct TspPlatformAudioDecoder TspPlatformAudioDecoder;
typedef struct TspOs TspOs;

typedef enum TspBitrate {
  kTspBitrate_Low = 0,      // 96 kbit
  kTspBitrate_Normal = 1,   // 160 kbit
  kTspBitrate_High = 2,     // 320 kbit
  kTspBitrate_24kbit = 3,   // 24 kbit
  kTspBitrate_48kbit = 4,   // 48 kbit
} TspBitrate;

//*********************************************************
//*** ERROR CODES
//*********************************************************
enum TspError {
  // Indicates something is not needed.
  kTspErrorNotNeeded = 3,

  // Not really an error, but indicates that something is still in progress, such 
  // as reading from a socket and all data is not available yet
  kTspErrorInProgress = 1,

  // Successful
  kTspErrorOk = 0,
  
  // This return value indicates that an operation started successfully,
  // and the completion callback will get called later.
  kTspErrorCompletionCallback = -1,

  // A generic temporary error. If you re-do the call later it might work.
  kTspErrorTemp = -2,

  // Indicates too many requests at the same time. Redo the same call later
  // after some requests have finished.
  kTspErrorTempTooMany = -3,

  // Indicates there's no more buffer space to perform the call. Retry the
  // same call when some space is available.
  kTspErrorTempSpace = -4,
    
  // Generic permanent error
  kTspErrorFail = -10,
  
  // Incorrect parameter
  kTspErrorParameter = -11,

  // No such entry
  kTspErrorNotFound = -12,

  // Incorrect URI
  kTspErrorInvalidUri = -13,

  // The file format is invalid.
  kTspErrorInvalidFormat = -14,

  // The file format is not supported
  kTspErrorUnsupportedFormat = -15,

  // The codec is not supported
  kTspErrorUnsupportedCodec = -16,

  // Memory allocation failed
  kTspErrorMemory = -17,

  // This request can not be performed because we're not logged in.
  kTspErrorNotLoggedIn = -18,

  // A generic socket error. The OS error code is available.
  kTspErrorSocket = -20,

  // The socket was closed prematurely.
  kTspErrorSocketClosed = -21,

  // The socket creation failed.
  kTspErrorSocketCreate = -22,

  // Library has not been initialized properly.
  kTspErrorUninitialized = -23,

  // Something timed out.
  kTspErrorTimeout = -24,

  // File create failed
  kTspErrorFileCreate = -25,
  
  // Disconnection was forced
  kTspErrorForcedDisconnect = -42,

  // Error the disk is full.
  kTspErrorDiskFull = -43,

  // Error, too many files.
  kTspErrorTooManyFiles = -44,

  // Error, too many keys.
  kTspErrorTooManyKeys = -45,

  // Bad http headers
  kTspErrorHttpHeaders = -46,

  /////////////////////
  // The following errors are access point related:
  // The Packet MAC (Message Authentication Code) was wrong
  kTspErrorApMac = -50,

  // The protocol signature was wrong.
  kTspErrorApSignature = -51,

  // Access point doesn't speak the right protocol
  kTspErrorApProtocol = -52,

  // Channel header from access point was broken
  kTspErrorApBadHeaders = -53,

  // The packet is too big
  kTspErrorApTooBigPacket = -54,

  // Something went wrong in protocol handshake
  kTspErrorApHandshake = -55,

  // Unable to parse hermes header
  kTspErrorApHermesHeader = -56,

  // Backend disconnects us.
  kTspErrorApDisconnect = -57,
  
  // Range 100-199 is reserved for login error codes from the backend.
  // Unable to authenticate to AP
  kTspErrorLoginProtocol = -100,

  // AP is overloaded.
  kTspErrorTryAnotherAp = -102,

  // You may not login from this country
  kTspErrorTravelRestriction = -109,

  // Premium account required
  kTspErrorPremiumRequired = -111,

  // The supplied login credentials were incorrect
  kTspErrorBadCredentials = -112,

  // Temporary error while verifying credentials
  kTspErrorTempCredentials = -113,

  // The application is banned
  kTspErrorApplicationBanned = -119,

  // Fallback login failure
  kTspErrorLoginGeneric = -199,

  // Range 200-299 is reserved for key errors
  kTspErrorKeyPermanent = -200,
  kTspErrorKeyTransient = -201,
  kTspErrorKeyRateLimited = -202,
  kTspErrorKeyUnableToPlay = -203,
  kTspErrorKeyCappingReached = -204,
  kTspErrorKeyTrackCapReached = -205,

  // Range 300-399 is reserved for channel errors
  kTspErrorChannelPermanent = -300,
  kTspErrorChannelTransient = -301,
  kTspErrorChannelRateLimit = -302,
  kTspErrorChannelRequestNotAllowed = -303,

  // Range 1000-1999 is reserved for hermes/http errors
  kTspHermesErrorFirst = -1000,
  kTspHermesErrorLast = -1999,
};

typedef enum TspError TspError;

enum {
  // Flush any existing buffers
  kTspAudioFlag_FlushBuffer = 1,
  
  // This flag is clear if playback should be paused
  kTspAudioFlag_Pause = 2,
};

typedef struct TspSampleFormat {
  // Either 1 = mono (rare) or 2 = stereo (common).
  int channels;
  // Sample rate in Hz (such as 22050, 44100 or 48000).
  int sample_rate;

  // Replaygain track / album values
  float replaygain_track, replaygain_album;
} TspSampleFormat;

// This callback gets called whenever audio data is available for playback.
// It returns the number of samples that were written, must never block, but it
// may return 0 if it can't handle the bytes right now.
// Another call to this function will then be done next time |TspDoWork| is called.
// |sample_format| contains the sample rate / channels of the provided buffer. This
// can change between songs. It may be NULL if no samples are provided.
// |samples_buffered| should be set to the number of samples buffered waiting to be played
// after this call returns. It may be NULL.
// If |flags| & kTspAudioFlag_FlushBuffer, the internal sample buffer should be flushed.
typedef int TspAudioCallback(void *context, int flags,
                             const TspSampleType *samples, int samples_size,
                             const TspSampleFormat *sample_format,
                             int *samples_buffered);

// This callback gets called to retrieve the current time in milliseconds,
// the returned value must be monotonous, and is allowed to wrap when it goes above 2**32.
typedef TspUint32 TspTimeCallback(void *context);
                                    
// Which type of event is delivered through the callback.
typedef enum TspCallbackEvent {
  // We have successfully connected to the backend, also used on reconnect.
  // This is also a reply to TspLogin when offline login is not used.
  kTspCallbackEvent_Connected = 1,
  
  // There was an error connecting to the backend, or a failed reconnect attempt.
  // More login attempts will be performed. Use |TspGetConnectionError| to see the error code.
  kTspCallbackEvent_ConnectError = 2,
  
  // The connection to the backend has been lost. Use |TspGetConnectionError| to see the error code.
  kTspCallbackEvent_Disconnected = 3,

  // This is a reply to TspLogin when offline login is used and when we successfully logged in.
  kTspCallbackEvent_OfflineLoginComplete = 4,

  // This is a reply to TspLogin that failed. You need to call TspLogin again cause we can't continue.
  kTspCallbackEvent_LoginError = 5,

  // Shuffle or repeat changed.
  kTspCallbackEvent_PlayControlsChanged = 6,
  
  // The local playback volume was changed.
  kTspCallbackEvent_VolumeChanged = 7,

  // The currently playing track has changed. Use |TspPlayerGetNowPlaying| to see which track.
  kTspCallbackEvent_NowPlayingChanged = 8,
  
  // The call to |TspPlayerSeek| is now done.
  kTspCallbackEvent_SeekComplete = 9,
  
  // The play token was lost, and playback has been paused. (Only one device can play
  // through Spotify at a time).
  kTspCallbackEvent_PlayTokenLost = 12,

  // Called when image data is delivered. This callback will get called multiple times
  // for the same image, once for every piece. |source| is the value passed to |TspDownloadImage|
  // and param points to an |TspImageDownloadResult| struct containing information about
  // the download.
  kTspCallbackEvent_ImageDownload = 13,

  // Called when playback has paused.
  // When you get the |kTspCallbackEvent_Pause|, pause your audio pipeline.
  kTspCallbackEvent_Pause = 14,

  // Called when playback has resumed after a pause.
  // When you get |kTspCallbackEvent_Resume|, DON'T resume your audio pipeline,
  // instead wait for the audio callback to get called, otherwise you won't know
  // if you should flush your buffer or not.
  kTspCallbackEvent_Resume = 15,

  // This callback will get send every time the user does something that triggers activity,
  // use that to reset any operating system idle timers. Use |TspIsActive| to query
  // if we're active.  Parameter is a TspBool specifying whether device was previously
  // active before this event.
  kTspCallbackEvent_Activated = 16,

  // This callback gets sent if the device is not active anymore (for example because
  // another device is now playing.) Use |TspIsActive| to query if we're active.
  kTspCallbackEvent_Deactivated = 17,

  // The player state changed because of an update from a remote device.
  // This also includes the volume of the currently playing remote device.
  kTspCallbackEvent_RemoteUpdate = 18,

  // A device list changed (or attributes of any device changed).
  kTspCallbackEvent_RemoteDeviceChanged = 19,
  
  // The list of tracks in a item list changed. |source| points at the item list.
  // Use |TspItemListGetError| to check for load errors.
  kTspCallbackEvent_ItemListChanged = 20, 

  // The URI of the item list has changed. |source| points at the item list,
  kTspCallbackEvent_ItemListUriChanged = 22, 

  // Called when the rootlist (list of playlists) has changed.
  kTspCallbackEvent_RootlistChanged = 23,

  // Called when a playlist change has finished. Note. it might
  // still have failed, call |TspPlaylistGetError| to see the error.
  kTspCallbackEvent_PlaylistChangeDone = 24,

  // Called when the end of fallback is reached in trackplayer.
  // (Used for internal tinspotify logging only)
  kTspCallbackEvent_EndFallback = 25,

  // Called when there was an offline sync error
  kTspCallbackEvent_OfflineSyncError = 26,

  // Called when offline sync state has changed for an item
  kTspCallbackEvent_OfflineSyncChanged = 27,

  // Called when the collection has changed.
  kTspCallbackEvent_CollectionChanged = 30,

  // Hermes callback
  kTspCallbackEvent_Hermes = 31,

  // Auto complete result is now available (or failed)
  kTspCallbackEvent_AutoComplete = 32,
} TspCallbackEvent;

// This callback gets called when some asynchronous event is delivered from TinySpotify
// to the consumer of the API.
typedef void TspCallback(void *context, TspCallbackEvent event, void *source, void *param);


// List of some special URIs that can be used with track lists to go to various special
// views.
// Loads the ItemList with the currently logged in user's list of playlists.
// NOTE: The internal list of playlists is not loaded until you get the first
//       kTspCallbackEvent_RootlistChanged notification, so then the view would become empty.
#define kTspUri_Root        "<ROOT>"

// Current user's playlist, but only one level. Not recursive.
#define kTspUri_RootOneLevel "<ROOT>/"

// Loads the ItemList into trackradio mode, where it requests radio tracks with the tracks
// setup with |TspItemListSetRadioTracks| as the seed
#define kTspUri_TrackRadio  "<TRACKRADIO>"

// Loads the ItemList with a list of all the radio genres that exist.
#define kTspUri_GenreRadioList   "<GENRE>"

//*********************************************************
//*** LOGIN & SESSION MANAGEMENT
//*********************************************************

// Bitmasks that can be passed to |flags| of |TspCreate|.
// Keep the in RAM root list up-to-date
#define kTspCreate_DownloadRootlist 1
// Disable the play token functionality, meaning that the device can play 
// without being interrupted by other players.
#define kTspCreate_DisablePlayToken 2
// Enable UDP based zeroconf mechanism
#define kTspCreate_EnableUdpZeroConf 4
// Disable AP signature check (required to run against non-production AP)
#define kTspCreate_DisableApSignature 8
// Disable AP resolve
#define kTspCreate_DisableApResolve 32
// Disable network activity
#define kTspCreate_DisableNetwork 64
// Enable the offline sync module
#define kTspCreate_EnableOfflineSync 128
// Act as a passive Spotify Connect member, never sending out any updates.
#define kTspCreate_PassiveSpotifyConnect 256
// Keep collection synced
#define kTspCreate_DownloadCollection 512

// Create an instance of TinySpotify.
// All the important memory is allocated from inside of this function, there are
// some other functions in the API that also allocate memory and they are
// clearly marked.
// |ident| is an internal ID of this device. It needs to be globally unique like a MAC-address,
//         and be between 1-63 characters.
// |callback| and |context| are used to return asynchronous events from TinySpotify.
// |appkey| and |appkey_size| points at the appkey given to you by Spotify.
// |user_agent| is a string with the name of the product name and model
//              TinySpotify is running on, max 63 characters.
// |limits| controls the sizes of certain internal memory buffers that are allocated
//          once at initialization time, pass NULL to use default limits.
// |os| can be used to provide another OS implementation instead of the default one.
// Returns NULL if not enough memory was available or if parameters are invalid.
Tsp *TspCreate5(const char *ident,
                TspCallback *callback, void *context,
                const void *appkey, size_t appkey_size,
                const char *user_agent,
                int flags, TspMemoryLimits *limits, TspOs *os);

// Simple versioning mechanism.
#define TspCreate TspCreate5

// Setup the login credentials and initiate a connection to the backend.
// If credentials are changed while being logged in, it will forcefully logout first.
#define kTspCredentialType_Password 0
#define kTspCredentialType_Token 1
#define kTspCredentialType_RememberMeBlob 2
#define kTspCredentialType_OAuth 3
// Enable the offline mode - allowing future login while offline.
#define kTspLogin_EnableOfflineMode 0x100
// When offline mode is enabled, also saves the password, removing the need to
// provide it later. Even when the password is not saved, a short password
// verifier is saved (required to log in offline). If the password doesn't match
// the stored password, it will perform an online login.
#define kTspLogin_RememberPassword 0x200
// If |TspSetOffline| has been used to deactivate the internet connection, then
// online login will fail with kTspErrorForcedDisconnect.
TspError TspLogin(Tsp *tsp, const char *username, const char *password, int flags);

// Disconnect the network connection.
void TspSetOffline(Tsp *tsp, TspBool offline);

// Log out from Spotify and disconnect the network connection. This is the opposite of
// |TspLogin|. If you want to login again, call |TspLogin|.
#define kTspLogout_ForgetPassword 1
void TspLogout(Tsp *tsp, int flags);

// Returns a string with the version of the TSP library.
const char *TspGetVersionString();

// Returns a stored token, suitable to be passed in TspLogin with kTspCredentialType_Token.
// |token_size| needs to be at least 1024 bytes big.
// Will fail if no token is available, tokens are normally available after a successful login.
// Both the |username| passed to |TspLogin| and the |token| returned by this function
// need to be passed to |TspLogin| in order to use the token.
TspError TspGetCredentialToken(Tsp *tsp, char *token, int token_size);

// Set the device's display name, used in GAIA. The first time the display name is set, GAIA will
// be initialized, while subsequent calls merely change the display name.
// |display_name| is a name we display to the user. Truncated after 63 chars.
// Returns an error code on error.
TspError TspSetDisplayName(Tsp *tsp, const char *display_name);

// Destroy TinySpotify. It is not allowed to delete TinySpotify from inside
// one of its callbacks.
void TspDestroy(Tsp *tsp);

// Return the default OS abstraction
TspOs *TspGetOs(Tsp *tsp);

// Setup a storage area, it is used for faster retrieval of bytes.
// If |multithreaded_io| is true, then all IO will happen on a background thread.
TspError TspInitializeStorage(Tsp *tsp, TspUint64 max_size, const char *path);

// Override the default access point.
TspError TspSetAccessPoint(Tsp *tsp, const char *hostname, int port);

// Set the bitrate to play. Will not take effect until tracks are reloaded from the backend.
TspError TspSetTargetBitrate(Tsp *tsp, TspBitrate bitrate);

// Setup a callback that will receive audio samples. If no callback is setup,
// a default audio player will be used (if available on the current platform).
TspError TspSetAudioCallback(Tsp *tsp, TspAudioCallback *callback, void *context);

// Call this function every iteration in the runloop to let TSP perform internal tasks.
// Returns the number of milliseconds until TspDoWork needs to be called again.
int TspDoWork(Tsp *tsp);

// Called in the runloop to wait for network events using the default network loop's select().
// Waits for at most |max_wait| milliseconds.
TspError TspWaitForNetworkEvents(Tsp *tsp, int max_wait);

// Returns the canonical username of the currently logged in user. If no user is
// logged in, returns the empty string.
const char *TspGetCanonicalUsername(Tsp *tsp);

// Returns the most recent connection error. If username/password is wrong, that is
// returned here.
TspError TspGetConnectionError(Tsp *tsp);

// Returns an offline sync error.
TspError TspGetOfflineSyncError(Tsp *tsp);

// Returns the operating system error code for the most recent connection error, if any.
int TspGetOsConnectionError(Tsp *tsp);

// Returns in how many seconds Spotify will try to reconnect, if we will never 
// try to reconnect because of a permanent error, returns -1.
int TspGetReconnectTime(Tsp *tsp);

// Returns true if we're currently connected to the backend.
TspBool TspIsConnected(Tsp *tsp);

// Returns true if this device is the currently active device. 
// (useful with gaia remote controlling.)
TspBool TspIsActive(Tsp *tsp);

// Return a search uri given a search query
const char *TspGetSearchUri(Tsp *tsp, const char *query);

// Return a starred uri for a given user, if username is NULL, returns the current user's
// starred uri.
const char *TspGetStarredUri(Tsp *tsp, const char *username);

// Converts the specified URI into a radio URI. |uri| may be either an artist or an album uri.
const char *TspGetRadioUri(Tsp *tsp, const char *uri);

enum {
  // If I have write access to the playlist
  kTspUriInfo_Writable = 1,
  // Set to true if the playlist is marked as public
  kTspUriInfo_Public = 2,
  // If I have ownership of this playlist URI
  kTspUriInfo_Owned = 4,
  // If I'm following this playlist
  kTspUriInfo_Following = 8,
};
// Retrieve information about a URI
int TspGetUriInfo(Tsp *tsp, const char *uri, int requested_info);

// Returns true if the item is blocked, i.e. will not be played automatically.
TspBool TspIsItemBlocked(Tsp *tsp, TspItem *item);

// Set whether the item is now blocked. Only a fixed number of items
// may be blocked. Blocked will not be played automatically in item lists, but
// they may be played if the user explicitly clicks them.
TspError TspSetItemBlocked(Tsp *tsp, TspItem *item,  TspBool blocked);

// Internal function to retrieve network traffic statistics
typedef struct TspNetworkStats {
  TspUint32 bytes_in, packets_in;
  TspUint32 bytes_out, packets_out;
} TspNetworkStats;
const TspNetworkStats *TspGetNetworkStats(Tsp *tsp);

// Start previewing the specified track. Specify NULL |track| to abort preview.
TspBool TspAudioPreview(Tsp *tsp, TspItem *track);

//*********************************************************
//*** REMOTE CONTROLLING
//*********************************************************

// Retrieve the name of a remote device, or NULL if no device
// exists with that index. Indexes go from 0..
const char *TspGetRemoteDeviceName(Tsp *tsp, int index);

// Retrieve the ID of a remote device, or NULL if no device
// exists with that index. Indexes go from 0..
const char *TspGetRemoteDeviceId(Tsp *tsp, int index);

// Number of devices
int TspGetRemoteDeviceCount(Tsp *tsp);

// Retrieve the index of the currently active remote device, or -1 if
// the local player is active.
int TspGetActiveRemoteDevice(Tsp *tsp);

//*********************************************************
//*** COLLECTION
//*********************************************************

// Returns true if the item is in the current user's collection.
TspBool TspCollectionContains(Tsp *tsp, TspItem *item);

// Add an item to the collection
TspError TspCollectionAdd(Tsp *tsp, TspItem *item);

// Remove an item from the collection
TspError TspCollectionRemove(Tsp *tsp, TspItem *item);

//*********************************************************
//*** MULTITHREADED AUDIO DECODER
//*********************************************************
// When the multithreaded decoder is active, TinySpotify does all work as usual,
// but audio deocding is not performed from inside TspDoWork, instead you need to 
// call TspAudioThreadingDoWork from your audio thread.
// NOTE: A locking mechanism is implemented through TspAudioThreadDelegate.
typedef struct TspMutex TspMutex;
struct TspMutex {
  // This method is called when TinySpotify acquires the audio lock.
  // While the lock is taken, do _NOT_ call the functions below.
  void (*lock)(TspMutex *mutex);
  // This method is called when TinySpotify releases the audio lock.
  void (*unlock)(TspMutex *mutex);
  // This method is called when TinySpotify informs that the audio sleep must wake the audio thread.
  void (*wakeup)(TspMutex *mutex);
};

// Enable multi threading of the audio pipeline. The |delegate| will get called when needed.
// You may not change the audio threading while playback is active.
TspBool TspEnableAudioThreading(Tsp *tsp, TspMutex *mutex);

// This is the only method that may be called from the audio thread.
// NOTE: You may _NOT_ call this if Tsp has the lock.
// Returns 'true' if audio streaming is currently active, and we should repeatedly call this. 
// If 'false' is returned you should sleep until |wakeup| is called.
TspBool TspAudioThreadingDoWork(Tsp *tsp);

//*********************************************************
//*** ROOT LIST
//*********************************************************

// Type of an item.
// Internal Note: These map 1:1 to the internal uri type.
typedef enum TspItemType {
  kTspItemType_Invalid = 0,
  kTspItemType_Album = 1,
  kTspItemType_Playlist = 2,
  kTspItemType_Search = 3,
  kTspItemType_Track = 4,
  kTspItemType_GenreRadioList = 7,
  kTspItemType_GenreRadio = 8,
  kTspItemType_AlbumRadio = 9,
  kTspItemType_ArtistRadio = 10,
  kTspItemType_TrackRadio = 11,
  kTspItemType_OpenFolder = 16,
  kTspItemType_ClosedFolder = 17,
  kTspItemType_Browse = 18,
  kTspItemType_Collection = 19,
} TspItemType;

// Return the number of elements in the rootlist
int TspGetRootlistCount(Tsp *tsp);

// Returns the type of item this is
TspItemType TspGetRootlistType(Tsp *tsp, int idx);

// Returns the URI to the idx:th element of the rootlist
const char *TspGetRootlistUri(Tsp *tsp, int idx);

// Returns the name to the idx:th element of the rootlist
const char *TspGetRootlistName(Tsp *tsp, int idx);

// Returns the indentation level of the idx:th element of the rootlist.
int TspGetRootlistIndent(Tsp *tsp, int idx);

int TspGetRootlistUriInfo(Tsp *tsp, int idx);

//*********************************************************
//*** PLAYBACK
//*********************************************************

// Start playing a specific track. If the context is currently paused, it will remain paused.
// This will not start playing directly, the uri will be resolved first.
void TspPlayerPlayUri(Tsp *tsp, const char *uri);

// Play a specific track. This will start playing directly.
void TspPlayerPlayTrack(Tsp *tsp, TspItem *item, int offset_ms);

// Start playing a specific track from a specific context. |context_uri| or |track_uri|
// may be NULL, then the code will do a best effort.
TspError TspPlayerPlayContext(Tsp *tsp, const char *context_uri, const char *track_uri, int index);

// Play the track at a specific index in the current item list. This call is asynchronous
// and the track will first be fetched from the backend and then playback will start.
void TspPlayerPlayIndex(Tsp *tsp, int index, int offset_ms);

// Play the track at a specified index in another item list, this will copy |tl|
// into the trackplayer's item list and start playing. |tl| will not be used again after this
// call has returned.
void TspPlayerPlayItemList(Tsp *tsp, TspItemList *tl, int index);

// Go to the next track. This call is asynchronous and the next track will first be fetched
// from the backend and then playback will start.
void TspPlayerNextTrack(Tsp *tsp);

// Go to the previous track. This call is asynchronous and the next track will first be fetched
// from the backend and then playback will start.
void TspPlayerPrevTrack(Tsp *tsp);

// Seek to a specific position given in milliseconds in the currently playing track
TspError TspPlayerSeek(Tsp *tsp, int position_ms);

// Either resume from a pause, or start playing the current track in the item list.
void TspPlayerPlay(Tsp *tsp);

// Pause playback
void TspPlayerPause(Tsp *tsp);

// Stop playback
void TspPlayerStop(Tsp *tsp);

typedef enum TspPlayState {
  // No track is currently playing, but the player might still have
  // a track that will start playing once |TspPlayerPlay| is called.
  kTspPlayState_Stopped = 0,

  // A track is currently playing
  kTspPlayState_Playing = 1,

  // A track is currently playing, but paused
  kTspPlayState_Paused = 2,
} TspPlayState;

TspPlayState TspPlayerGetState(Tsp *tsp);

// Returns whether we're currently in the middle of initiating playback.
// For example while a URI is resolved but before it has started playing.
TspBool TspPlayerIsLoading(Tsp *tsp);

// Return the metadata about the currently playing track.
// If the state is kTspPlayState_Stopped, it returns the track that will start
// playing once we do TspPlayerPlay. If no such track exists, returns NULL.
TspItem *TspPlayerGetNowPlaying(Tsp *tsp);

// Set the playing index in the now playing list. If a track is currently playing,
// this sets which track will play next.
void TspPlayerSetNowPlayingIndex(Tsp *tsp, int track);

// Return a pointer to the item list representing the currently playing context. This
// function never returns NULL, as there always exists an internal player ItemList.
// The returned object is owned by TinySpotify, and calling methods on this ItemList directly
// may interfere with the operation of the TrackPlayer.
TspItemList *TspPlayerGetItemList(Tsp *tsp);

// Returns the i:th future track to be played. Returns NULL if no further future exists.
TspItem *TspPlayerGetFuture(Tsp *tsp, int i);

// Return the i:ht queued track. Returns NULL if no more queued items exist.
TspItem *TspPlayerGetQueued(Tsp *tsp, int i);

// Return the duration of the currently playing track, in seconds, rounded appropriately.
// Returns -1 if the duration is not known yet.
int TspPlayerGetDuration(Tsp *tsp);

// Return the position of the currently playing track, in seconds, rounded appropriately.
// The returned value is always positive, and starts at zero.
int TspPlayerGetPosition(Tsp *tsp);

// Same as TspPlayerGetDuration but the returned value is in milliseconds,
// in case higher precision is needed.
int TspPlayerGetDurationMs(Tsp *tsp);

// Same as TspPlayerGetPosition but the returned value is in milliseconds,
// in case higher precision is needed.
int TspPlayerGetPositionMs(Tsp *tsp);

// Return the bitrate of the compressed audio for the currently playing track,
// the returned value is given in kilobits (e.g. 160 = 160 kbit).
// If the bitrate is currently unknown, returns zero.
int TspPlayerGetBitrate(Tsp *tsp);

// Return the error code of the most recent playback operation
TspError TspPlayerGetError(Tsp *tsp);

// Returns the current repeat mode
int TspPlayerGetRepeat(Tsp *tsp);

// Set the current repeat mode
TspError TspPlayerSetRepeat(Tsp *tsp, int repeat);

// Get the shuffle mode.
int TspPlayerGetShuffle(Tsp *Tsp);

// Set the shuffle mode
TspError TspPlayerSetShuffle(Tsp *tsp, int shuffle);

// Return the current volume. Returns a value between 0-65535.
// Returns zero if volume is muted.
int TspPlayerGetVolume(Tsp *tsp);

// Set the volume as an integer between 0-65535.
TspError TspPlayerSetVolume(Tsp *tsp, int volume);

// Returns true if volume is muted.
TspBool TspPlayerGetMute(Tsp *tsp);

// Set whether volume is muted. When muted, TinySpotify will
// internally pause playback after a little while, and resume when unmuted.
TspError TspPlayerSetMute(Tsp *tsp, TspBool mute);

enum {
  kTspEqualizer_Enable = 1,
  // Apply gain automatically. Equalizer must be enabled for these to have an effect
  kTspEqualizer_AutoGain_Track = 2,
  kTspEqualizer_AutoGain_Album = 4,
};

// Configure the internal equalizer. 
// |pregain| is a value between -12db - 12db that controls how much pregain to apply.
// |bands| is a gain between -12db - 12db that controls how much to attenuate each band,
//   the frequencies are 60hz, 170hz, 310hz, 600hz, 1khz, 3khz, 6khz, 12khz, 14khz, 16khz
void TspPlayerSetEqualizer(Tsp *tsp, int enable, float pregain, const float bands[10]);

enum {
  // Move playback to the specified device.
  kTspSetDeviceFlag_MovePlayback = 1,
};

#define kTspDeviceIdAuto ((const char*)1)

// Set the device id currently controlled by the player APIs.
// Set |device_id| to kTspDeviceIdAuto to control the device that's currently playing.
// Set |device_id| to NULL to control the local device.
// Set |flags| to kTspSetDeviceFlag_MovePlayback to move playback.
void TspPlayerSetDevice(Tsp *tsp, const char *device_id, int flags);

//*********************************************************
//*** ITEM
//*********************************************************

// Returns the name of the track. Never returns NULL.
const char *TspItemGetName(TspItem *item);

// Returns the URI of the item. The returned string is valid until
// the next call to TinySpotify. Never returns NULL.
const char *TspItemGetUri(TspItem *item, Tsp *tsp);

// Returns the artist name of the item. Never returns NULL.
const char *TspItemGetArtistName(TspItem *item);

// Returns the URI of the item's artist or empty. The returned string is valid until
// the next call to TinySpotify. Never returns NULL.
const char *TspItemGetArtistUri(TspItem *item, Tsp *tsp);

// Returns the album name of the item. Never returns NULL.
const char *TspItemGetAlbumName(TspItem *item);

// Returns the URI of the items's album or empty. The returned string is valid until
// the next call to TinySpotify. Never returns NULL.
const char *TspItemGetAlbumUri(TspItem *item, Tsp *tsp);

// Returns the uri to the image. Returns empty string if there is no uri.
const char *TspItemGetImageUri(TspItem *item, Tsp *tsp);

// Returns true if the item is a track that is playable.
TspBool TspItemIsPlayable(TspItem *item);

// Returns true if the item is a track that is manually queued
TspBool TspItemIsQueued(TspItem *item);

// Set if the item is manually queued (Use only for testing)
void TspItemSetQueued(TspItem *item, TspBool queued);

// Returns true if the item represents a relinked track, i.e. 
// the actual track will not be played due to permissions 
// but another similar one will get played instead.
// If |uri_buffer| is non-NULL then when this function returns
// true, the relinked track link gets stored there.
#define kTspMaxSizeOfRelinkedUri 64
TspBool TspItemIsRelinked(TspItem *item, char uri_buffer[kTspMaxSizeOfRelinkedUri]);

// Returns true if the item is valid, if it's not valid, it can't be played or visualized.
// An item is not valid for example if backend returned no metadata for the item.
// All the methods above will return appropriate values (empty strings) for invalid items,
// so there's not much point in calling this method unnecessarily.
TspBool TspItemIsValid(TspItem *item);

// Returns the type of an item in a list.
TspItemType TspItemGetType(TspItem *item);

// Returns the length in seconds of the track. This value is for display purposes
// only and may differ from the duration once the track actually starts playing.
// For playlists in the root list, this returns the number of tracks in the playlist.
int TspItemGetLength(TspItem *item);

// Return the number of people that follow a playlist, only defined for playlists.
int TspItemGetFollowCount(TspItem *item);

// Duplicate an item, so that it can be saved until later. Will allocate memory, returns
// NULL if no memory is available.
TspItem *TspItemDuplicate(TspItem *item, Tsp *tsp);

// Destroy a previously duplicated item returned from |TspItemDuplicate|.
void TspItemDestroy(TspItem *item, Tsp *tsp);

int TspItemGetFlags(TspItem *item);

//*********************************************************
//*** ITEMLIST
//*********************************************************

// Creates a new item list, can be used to load playlists or resources. Will allocate memory,
// returns NULL if no memory is available.
TspItemList *TspItemListCreate(Tsp *tsp, int item_size);

// Destroys an existing item list created with |TspItemListCreate|.
void TspItemListDestroy(TspItemList *tl);

// Set the currently loaded resource, will get loaded from offset |load_offset| and forward.
// |uri| is a spotify URI or one of the magic values starting with kTspUri_*
TspError TspItemListLoad(TspItemList *tl, const char *uri, int load_offset);

// Manually assign a list of tracks to an item list, and initiate load of the metadata.
// If one or more of the uris are not valid track URIs, then blank elements are inserted.
TspError TspItemListLoadManual(TspItemList *tl, const char * const *uris, int num_uris);

// Make one itemlist identical to another itemlist
TspError TspItemListCopyFrom(TspItemList *tl, TspItemList *src);

// Setup which tracks are used as seeds for the track based radio, this makes a
// random selection of the tracks currently visible in the item list.
void TspItemListSetRadioTracks(TspItemList *tl);

// Returns a user displayable name of the item list. Returns empty string
// if no name is available. Never returns NULL.
const char *TspItemListGetName(TspItemList *tl);

// Returns a user displayable name of the owner of the item list. Returns empty string
// if no name is available. Never returns NULL. For playlists this returns
// the name of the owner.
const char *TspItemListGetOwnerName(TspItemList *tl);

// Returns the URI of the item list. The returned string is valid until
// the next call to TinySpotify. Never returns NULL.
const char *TspItemListGetUri(TspItemList *tl);

// Return info about the URI of the itemlist (See TspUriInfo)
int TspItemListGetUriInfo(TspItemList *tl, int requested_info);

// Return the type of the resource loaded in the item list.
TspItemType TspItemListGetType(TspItemList *tl);

// Returns the total number of items in the item list. If the number is unknown,
// returns -1. Returns the total size of the underlying resource, not all elements
// are necessarily loaded yet.
int TspItemListGetTotalLength(TspItemList *tl);

// Returns a pointer to the n:th item of the item list. If that index is outside the
// currently loaded range of the item list, or if the element could not be loaded,
// returns NULL.
TspItem *TspItemListGetItem(TspItemList *tl, int n);

// Return the index of the playing track that should be highlighted. Returns -1
// if no track is current. If playback is stopped, this function returns
// the track that will play once we start playing.
int TspItemListGetNowPlayingIndex(TspItemList *tl);

// Returns the index of the first loaded item (i.e. the first value for which 
// TspItemListGetitem will return non-NULL).
int TspItemListGetFirstVisibleItem(TspItemList *tl);

// Returns the index of the last loaded item. If this value is smaller than the
// value returned by |TspItemListGetFirstVisibleItem| then no items are loaded.
int TspItemListGetLastVisibleItem(TspItemList *tl);

// If the item list is currently loading more elements
TspBool TspItemListIsLoading(TspItemList *tl);

// Will make sure that item |first_item| gets loaded in view.
// Returns: kTspErrorOk if the request was serviced.
//          kTspErrorTempTooMany if the item list is already busy with another operation
//          kTspErrorInProgress if the item list started the request but it's not finished.
TspError TspItemListLoadRange(TspItemList *tl, int first_item, int num_items);

// Returns true if the specified index is currently playing.
TspBool TspItemListIsIndexPlaying(TspItemList *tl, int index);

// Return the error code of the most recent load operation
TspError TspItemListGetError(TspItemList *tl);

// Set the open/closed state of a folder
void TspItemListSetFolderOpen(TspItemList *tl, int i, int open);

// Return the indentation level of the specified item. This is used for the rootlist only,
// each item inside a folder will have an indentation level one step higher.
int TspItemListGetIndent(TspItemList *tl, int i);

typedef enum TspOfflineState {
  // Not offline synced
  kTspOfflineState_No = 0,
  // Is offline synced
  kTspOfflineState_Yes = 1,
  // Is currently syncing
  kTspOfflineState_Syncing = 2,
  // Is waiting to sync
  kTspOfflineState_Waiting = 3,
} TspOfflineState;

// Retrieve the current offline state for the specified index of the item list.
// If |i| is -1 the offline state for the item list itself is returned.
TspOfflineState TspItemListGetOfflineState(TspItemList *tl, int i);

// Set the offline available flag for the specified index of an item list. 
// If |i| is -1 the offline state for the itemlist itself is changed.
TspBool TspItemListSetOfflineState(TspItemList *tl, int i, TspBool available_offline);

// Return the name of the |i|:th subgenre. Returns NULL if i is invalid.
const char *TspItemListGetSubgenreName(TspItemList *tl, int i);

// Return the uri of the |i|:th subgenre. Returns empty string if |i| is invalid.
const char *TspItemListGetSubgenreUri(TspItemList *tl, int i);


//*********************************************************
//*** AUTO COMPLETE
//*********************************************************

// Initiate auto completion for the specified |prefix|. If an auto complete
// is already in progress, it is aborted.
void TspAutoComplete(Tsp *tsp, const char *prefix);

// Return the |i|:th auto complete response. If |i| is not a valid index, returns
// NULL.
const char *TspAutoCompleteGetResult(Tsp *tsp, int i);

//*********************************************************
//*** IMAGE RETRIEVAL
//*********************************************************
typedef struct TspImageDownloadResult {
  // kTspError_InProgress while getting data, or an error code on fail.
  TspError error;

  // The current chunk of the image. Accumulate these bytes yourself, and then
  // when you get the OK error code, it's done.
  const void *data;
  int data_size;
} TspImageDownloadResult;

typedef enum TspImageQuality {
  kTspImageQuality_60 = 0,  // 60x60 pixels
  kTspImageQuality_120 = 1, // 120x120 pixels
  kTspImageQuality_300 = 2, // 300x300 pixels


  kTspImageQuality_default = 2,
} TspImageQuality;

// Initiate an image download request for the specified |image_uri|. |source_ptr| is passed
// without modifications to the |kTspCallbackEvent_ImageDownload| callback.
// Internally there may only be a low number of image requests at a time, then the call
// will fail.
// |quality| is the desired image size.
TspError TspDownloadImage(Tsp *tsp, const char *image_uri, void *source_ptr, TspImageQuality quality);

// Cancel an ongoing download image call. No further callbacks will get called. It is valid
// to call this from inside the |kTspCallbackEvent_ImageDownload| callback.
void TspDownloadImageCancel(Tsp *tsp, void *source_ptr);

//*********************************************************
//*** PLAYLIST MODIFY
//*********************************************************

// Add a list of tracks to the specified playlist. 
// Note: Will allocate memory internally, and will fail if not enough memory is available.
enum {
  kTspPlaylistSimpleModify_AddLast = 0,
  kTspPlaylistSimpleModify_AddFirst = 1,
  kTspPlaylistSimpleModify_Remove = 2,
};

TspError TspPlaylistSimpleModify(Tsp *tsp, const char *playlist_uri,
                                 const char * const * uris, int num_uris,
                                 int operation);

// Returns the error code of the most recent playlist operation, suitable for being called
// inside of kTspCallbackEvent_PlaylistChangeDone.
TspError TspPlaylistGetError(Tsp *tsp);

// Follow or unfollow a playlist
TspError TspPlaylistSetFollow(Tsp *tsp, const char *playlist_uri, TspBool follow);

//*********************************************************
//*** PLATFORM AUDIO DECODER
//*********************************************************

typedef TspPlatformAudioDecoder *TspPlatformAudioDecoderFactory(void *context, TspUint32 ident);

enum {
  // The input packet was consumed. If this is not set, the input packet has
  // to be passed again on the next call to process.
  kTspPlatformAudioDecoder_ReadInput = 1,
  // An output packet was generated.
  kTspPlatformAudioDecoder_WroteOutput = 2,
  // Reached the end of stream.
  kTspPlatformAudioDecoder_EndOfStream = 4,
};

struct TspPlatformAudioDecoder {
  // Destroy decoder
  void(*destroy)(TspPlatformAudioDecoder *pad);
  // Initialize decoder
  TspError (*initialize)(TspPlatformAudioDecoder *pad, int channels, int sample_rate,
                         const unsigned char *extradata, int extra_data_size);
  // Flush decoder in preparation for a seek
  void (*reset)(TspPlatformAudioDecoder *pad);
  // Try to decode an input packet. Pass in an empty packet to signal end of stream.
  // Returns a negative code on failure, otherwise a combination of 
  // kTspPlatformAudioDecoder_ReadInput and kTspPlatformAudioDecoder_WroteOutput.
  int (*process)(TspPlatformAudioDecoder *pad, const unsigned char *data, int data_size,
                 TspSampleType **samples, int *samples_size);
};

void TspRegisterAudioDecoderFactory(Tsp *tsp, TspPlatformAudioDecoderFactory *factory, void *context);

//*********************************************************
//*** MEMORY LIMITS
//*********************************************************
// Contains sizes of various internal memory buffers that get allocated
// inside of TspCreate.
struct TspMemoryLimits {
  // Size of receive buffer in the AP communication. Set to zero to use default.
  int recv_window_size;
  
  // Size of send buffer size in the AP communication. Set to zero to use default.
  int send_window_size;
  
  // The number of tracks to load concurrently inside of the track player's item list.
  // Set to zero to use the default which is 100. Any playlists larger than 100 elements
  // will thus be loaded using paging.
  int track_player_size;
  
  // Size of the sample buffer used to store decompressed samples before they're
  // passed along to the audio driver. Set to zero to use the default.
  int sample_buffer_size;
  
  // Size of the buffer holding compressed audio samples. The default is 256kb.
  // The size of this buffer controls how long we may lose connectivity while playback
  // keeps playing. Set to zero to use default.
  int compressed_buffer_size;
  
  // Threshold controls at what interval to request new bytes from the backend, whenever
  // the number of buffered bytes drops below this value, another request is initiated.
  // Set to zero to use default.
  int compressed_buffer_threshold;
  
  // Controls the maximum size of a request for bytes to the backend.
  // Set to zero to use the default.
  int compressed_buffer_maxreqsize;
  
  // Total number of items that may be blocked.
  int blocked_list_size;

  // Number of rootlist items to keep in memory.
  int rootlist_items;
};

//*********************************************************
//*** DEBUG LOGGING
//*********************************************************
typedef void TspDebugLogger(const char *message, ...);

// Register a callback that gets any log messages from the library.
// This is not normally used, unless TinySpotify was built with debugging strings.
// All messages from TinySpotify get sent to the same log sink.
void TspSetDebugLogger(TspDebugLogger *logger);

//*********************************************************
//*** SOCKET MANAGER
//*********************************************************
// Implement this API to use your own socket manager instead of the
// default (BSD based) socket manager. The socket manager can be passed to
// TspSetSocketManager to avoid using the built-in socket manager.
typedef struct TspSocket TspSocket;

struct TspSocketMethods {
  // Close the socket
  void (*socket_close)(TspSocket *s);

  // Read up to |data_size| bytes into |data| from the socket.
  // Returns the number of bytes read, 0 if no bytes could be read, or -1 on EOF.
  // Returns another negative value on socket error, a more specific error code
  // is stored in the socket's |error| field.
  int (*socket_read)(TspSocket *s, unsigned char *data, int data_size);

  // Write up to |data_size| bytes of |data| from the socket.
  // Returns the number of bytes written, 0 if no bytes could be written.
  // Returns a negative value on socket error, a more specific error code
  // is stored in the socket's |error| field.
  int (*socket_write)(TspSocket *s, const unsigned char *data, int data_size);

  // Use this on listener sockets to accept a new connection
  TspSocket *(*socket_accept)(TspSocket *s);

  // Return the underlying OS handle for a socket
  int (*socket_get_oshandle)(TspSocket *s);
};

struct TspSocket {
  // Points at the socket's methods.
  const struct TspSocketMethods *methods;
  
  // If the socket is readable, this field should get cleared
  // from inside |socket_read| if the socket is no longer readable.
  TspBool readable;
  
  // If the socket is writable, this field should get cleared
  // from inside |socket_write| if the socket is no longer writable.
  TspBool writable;
  
  // The operating system error code if we had a socket error, otherwise zero.  
  int error;
};

struct TspSocketManagerMethods {
  // Call to destroy the instance of the socket manager (and shutdown any internal threads).
  void (*destroy)(TspSocketManager *tsm);

  // Create a tcp socket and connect it to (hostname, port)
  TspSocket *(*create_tcp_socket)(TspSocketManager *sm, const char *hostname, int port);

  // Create a tcp socket that listens on the specified port for incoming connections.
  // Returns NULL if socket can't be created.
  TspSocket *(*create_tcp_listen_socket)(TspSocketManager *sm, int port);

  // Create a udpv4 socket that optionally listens on the specified port. Set this pointer to NULL
  // if UDP sockets are not supported.  Returns NULL if socket can't be created.
  TspSocket *(*create_udp_listen_socket)(TspSocketManager *sm, int port);
  
  // Wait up to |maxwait| milliseconds for a network event. Set to NULL if this
  // call is not supported.
  // Note: Only the thread that created the socket manager is allowed to call this method.
  int (*wait_for_network_events)(TspSocketManager *sm, int maxwait);
  
  // Wake up the app's main thread, causing DoWork to get called.
  void (*wakeup_thread)(TspSocketManager *sm);

  // Used for extending the socket manager with extensions
  int (*ioctl)(TspSocketManager *sm, int what, void *value);

  // Initialize multi-threaded mode.
  void (*threading_initialize)(TspSocketManager *tsm, void(*wake_func)(void *arg), void *wake_arg);

  // Called before main thread goes to sleep, to tell the network thread to start waiting
  void (*threading_before_sleep)(TspSocketManager *tsm);

  // Called after the main thread wakes up, to register the events from the network 
  // thread with the main thread.
  void(*threading_after_sleep)(TspSocketManager *tsm);
};

struct TspSocketManager {
  const struct TspSocketManagerMethods *methods;
};

// Shorthand for calling threading_initialize
typedef void TspMultithreadedWakeFunc(void *arg);
void TspInitializeMultithreadedNetworking(Tsp *tsp, TspMultithreadedWakeFunc *wake_func, void *wake_arg);

//*********************************************************
//*** ZERO CONF
//*********************************************************
// Helper functions for ZeroConf API implementation.

// Don't touch the fields of this struct directly, instead use the accessors below.
typedef struct TspZeroConf {
  unsigned char cbin[96];
  char tmpbuf[1024];
  char reserved[1024 - 96];
} TspZeroConf;

// |sizeof_tzc| should be set to sizeof(TspZeroConf).
// |entropy| an array of random values of length |entropy_len|. It needs to be at least
// 16 bytes long. For security, it's critical that entropy contains good quality randomness.
// Return true on success.
TspBool TspZeroConfInit(TspZeroConf *tzc, int sizeof_tzc, const void *entropy, size_t entropy_size);

// Return a string representing our public key to be offered through an insecure channel.
// Lifetime of the returned value is until the next TspZeroConf* call. Returns NULL on failure.
const char *TspZeroConfPublicKey(TspZeroConf *tzc);

// Decrypt an encrypted blob from the client and return it as a string, this string
// can be passed to |TspLogin| with kTspCredentialType_RememberMeBlob.
// |client_key| is returned from the client who was offered our public key in string format.
// |encrypted_blob| is the encrypted blob obtained from the client in string format.
// Lifetime of the returned value is until the next TspZeroConf* call. Returns NULL on failure.
const char *TspZeroConfDecryptBlob(TspZeroConf *tzc, const char *client_key,
                                   const char *encrypted_blob);


//*********************************************************
//*** TRACK FILE READER
//*********************************************************
// This API provides a facility for reading the raw, compressed, data of a track.
// It can be used if you want to handle all steps of decompression and buffering
// yourself. Currently it's being used by 3rd party DJ integrations.
// The ogg files stored in Spotify's backend are prefixed by a special seek
// header. This API hides that seek header and return the actual ogg file instead.

// Create a TspTrackFileReader for a given |track|. Will allocate memory. This
// requests a play token from backend so any other devices will stop playback.
// Upon receiving a kTspCallbackEvent_PlayTokenLost event, you need to pause playback.
TspError TspTrackFileReader_Create(Tsp *tsp, TspItem *track, TspTrackFileReader **result);

// Destroy an instance of the track file reader.
void TspTrackFileReader_Destroy(TspTrackFileReader *tfr);

// Returns any error associated with the track file reader.
// This returns |kTspErrorInProgress| while the track is being prepared for reading.
TspError TspTrackFileReader_GetError(TspTrackFileReader *tfr);

// Returns the physical file size of the resource in bytes. Returns 0 if the track
// is still preparing.
TspUint32 TspTrackFileReader_GetFileSize(TspTrackFileReader *tfr);

// Returns the Replaygain track gain associated with the file.
float TspTrackFileReader_GetTrackGain(TspTrackFileReader *tfr);

// Returns the Replaygain album gain associated with the file.
float TspTrackFileReader_GetAlbumGain(TspTrackFileReader *tfr);

// Returns the total number of samples in the file.
TspUint32 TspTrackFileReader_GetSampleCount(TspTrackFileReader *tfr);

// Map a sample number to an approximate byte offset using the Spotify seek table.
TspUint32 TspTrackFileReader_MapSample(TspTrackFileReader *tfr, TspUint32 sample);

// Attempts to read bytes from a certain offset. Does not block, returns the number of 
// bytes immediately available. This will also trigger a play token request.
size_t TspTrackFileReader_Read(TspTrackFileReader *tfr, TspUint32 offset, void *buffer, size_t buffer_size);

// Deprectated functions
TspError TspSetCredentials(Tsp *tsp, const char *username, const char *password, int flags);


//*********************************************************
//*** MULTIPLE AUDIO STREAMERS
//*********************************************************

// Return the built-in audio streamer. You must not delete it.
TspAudioStreamer *TspPlayerGetAudioStreamer(Tsp *tsp);

// Creates another instance of an audio streamer, can be used
// to play many tracks in parallel.
TspAudioStreamer *TspAudioStreamerCreate(Tsp *tsp, int download_buffer_size, int sample_buffer_size);

// Destroy an audio streamer created with |TspAudioStreamerCreate|.
void TspAudioStreamerDestroy(TspAudioStreamer *tas);

// Start playing |track| in the audio streamer |tas|.
TspError TspAudioStreamerStart(TspAudioStreamer *tas, TspItem *track);

// Stop playing in the audio streamer |tas|.
void TspAudioStreamerStop(TspAudioStreamer *tas);

// Seek to an offset in the audio streamer
void TspAudioStreamerSeek(TspAudioStreamer *tas, TspUint32 sample_offset);

// This function can be used to read and decompress samples, bypassing
// the callback mechanism. Returns the number of TspSampleType read.
// |sample_format| gets filled with the returned sample format if the return value is nonzero.
// NOTE: In multi threaded mode, you may _NOT_ call this if Tsp has the lock.
int TspAudioStreamerRead(TspAudioStreamer *tas, TspSampleType *samples, int samples_size,
                         TspSampleFormat *sample_format);

TspUint32 TspAudioStreamerDuration(TspAudioStreamer *tas);


//*********************************************************
//*** RAW HERMES QUERIES
//*********************************************************
struct TspHermesCallbackData {
  TspError error;
  const void *data;
  int data_size;
  TspInt32 hermes_id;
};

#define kTspHermesMethod_GET 3

// Makes a hermes request. Returns a negative error code on failure, or a 
// positive request id.
TspInt32 TspHermesRequest(Tsp *tsp, int method, const char *uri, int timeout);
// Cancel a hermes request.
void TspHermesCancel(Tsp *tsp, TspInt32 req_id);

#endif  // TINY_SPOTIFY_H_

