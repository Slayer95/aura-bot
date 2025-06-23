/*

  Copyright [2025] [Leonardo Julca]

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */

#include "optional.h"
#include <filesystem>

#define EXPLICIT_OPT(T)\
template class OptReader<T>;\
template class OptWriter<T>;\
template OptReader<T> ReadOpt(const optional<T>& opt);\
template OptWriter<T> WriteOpt(optional<T>& value);

using namespace std;

//
// OptReader
//

template<typename T>
OptReader<T>::OptReader(const optional<T>& nSource)
 : source(ref(nSource))
{
};

template<typename T>
OptReader<T>::~OptReader()
{
};

template<typename T>
void OptReader<T>::operator>>(T& target) const
{
  const optional<T>& optSource = source.get();
  if (!optSource.has_value()) return;
  target = *optSource;
}

template<typename T>
void OptReader<T>::operator>>(optional<T>& target) const
{
  const optional<T>& optSource = source.get();
  if (!optSource.has_value()) return;
  target = *optSource;
}

//
// OptWriter
//

template<typename T>
OptWriter<T>::OptWriter(optional<T>& nTarget)
 : target(ref(nTarget))
{
};

template<typename T>
OptWriter<T>::~OptWriter()
{
};

template<typename T>
void OptWriter<T>::operator<<(const T& source)
{
  optional<T>& optTarget = target.get();
  optTarget = source;
}

template<typename T>
void OptWriter<T>::operator<<(const optional<T>& source)
{
  if (!source.has_value()) return;
  optional<T>& optTarget = target.get();
  optTarget = *source;
}

template<typename T>
OptReader<T> ReadOpt(const optional<T>& opt) {
  return {opt};
}


template<typename T>
OptWriter<T> WriteOpt(optional<T>& value) {
  return {value};
}

EXPLICIT_OPT(bool)
EXPLICIT_OPT(uint8_t)
EXPLICIT_OPT(uint16_t)
EXPLICIT_OPT(uint32_t)
EXPLICIT_OPT(int64_t)
EXPLICIT_OPT(string)
//EXPLICIT_OPT(sockaddr_storage)
EXPLICIT_OPT(Version)
EXPLICIT_OPT(filesystem::path)

// enums
EXPLICIT_OPT(CacheRevalidationMethod)
EXPLICIT_OPT(CrossPlayMode)
EXPLICIT_OPT(FakeUsersShareUnitsMode)
EXPLICIT_OPT(GameLoadingTimeoutMode)
EXPLICIT_OPT(GameObserversMode)
EXPLICIT_OPT(GamePlayingTimeoutMode)
EXPLICIT_OPT(GameResultSourceSelect)
EXPLICIT_OPT(GameSpeed)
EXPLICIT_OPT(GameVisibilityMode)
EXPLICIT_OPT(HideIGNMode)
EXPLICIT_OPT(LobbyOwnerTimeoutMode)
EXPLICIT_OPT(LobbyTimeoutMode)
EXPLICIT_OPT(LogLevel)
EXPLICIT_OPT(MirrorSourceType)
EXPLICIT_OPT(MirrorTimeoutMode)
EXPLICIT_OPT(OnIPFloodHandler)
EXPLICIT_OPT(OnPlayerLeaveHandler)
EXPLICIT_OPT(OnRealmBroadcastErrorHandler)
EXPLICIT_OPT(OnShareUnitsHandler)
EXPLICIT_OPT(OnUnsafeNameHandler)
EXPLICIT_OPT(PlayersReadyMode)
EXPLICIT_OPT(UDPDiscoveryMode)
EXPLICIT_OPT(W3ModLocale)

#undef EXPLICIT_OPT
