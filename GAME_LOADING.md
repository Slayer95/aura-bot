How game loading works
=======================

# Standard game load

EventGameStartedLoading:

```c++
  for (const CGameVirtualUser& fakeUser : m_FakeUsers) {
    AppendByteArrayFast(m_LoadingVirtualBuffer, fakeUser.GetGameLoadedBytes());
  }
```

EventUserAfterDisconnect:

```c++
  QueueLeftMessage(user)
  auto packet = SEND_W3GS_GAMELOADED_OTHERS(user->GetUID())
  AppendByteArrayFast(m_LoadingVirtualBuffer, packet);
  SendAll(packet);
```
  

EventUserLoaded:

```c++
  auto packet = SEND_W3GS_GAMELOADED_OTHERS(user->GetUID());
  AppendByteArrayFast(m_LoadingRealBuffer, packet);
  SendAll(packet)
```

EventGameBeforeLoaded:

```c++
  SendAll(GetOnlyFakeUsersLoaded(m_LoadingVirtualBuffer)) 
```

# Load-in-game

EventGameStartedLoading:

```c++
  for (const CGameVirtualUser& fakeUser : m_FakeUsers) {
    AppendByteArrayFast(m_LoadingVirtualBuffer, fakeUser.GetGameLoadedBytes());
  }

  for (const auto& user : m_Users) {
    vector<uint8_t> packet = GameProtocol::SEND_W3GS_GAMELOADED_OTHERS(user->GetUID());
    AppendByteArray(m_LoadingRealBuffer, packet);
  }
```

EventUserAfterDisconnect:

```c++
  QueueLeftMessage(user)
```

EventUserLoaded:

```c++
  Send(user, m_LoadingRealBuffer);
  Send(user, m_LoadingVirtualBuffer);
```
