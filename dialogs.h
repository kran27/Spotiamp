#include <string>
#include "window.h"
#include "spotifyamp.h"

bool ShowLoginDialog(PlatformWindow *parent, std::string *username, std::string *password);
std::string ShowSearchDialog(PlatformWindow *parent, Tsp *tsp);

void AutoCompleteCopy();