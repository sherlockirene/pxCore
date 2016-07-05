#include "rtRemote.h"
#include "rtRemoteClient.h"
#include "rtRemoteConfig.h"
#include "rtObjectCache.h"
#include "rtRemoteServer.h"
#include "rtRemoteStream.h"

#include <rtLog.h>
#include <mutex>
#include <thread>

static std::mutex gMutex;
static rtRemoteEnvironment* gEnv = nullptr;

rtRemoteEnvironment::rtRemoteEnvironment()
  : Config(nullptr)
  , Server(nullptr)
  , ObjectCache(nullptr)
  , StreamSelector(nullptr)
  , RefCount(1)
  , Initialized(false)
{
  Config = rtRemoteConfig::getInstance();

  StreamSelector = new rtRemoteStreamSelector();
  StreamSelector->start();

  Server = new rtRemoteServer(this);
  ObjectCache = new rtObjectCache(this);
}

rtRemoteEnvironment::~rtRemoteEnvironment()
{
}

void
rtRemoteEnvironment::shutdown()
{
  if (Server)
  {
    delete Server;
    Server = nullptr;
  }

  if (StreamSelector)
  {
    rtError e = StreamSelector->shutdown();
    if (e != RT_OK)
      rtLogWarn("failed to shutdown StreamSelector. %s", rtStrError(e));

    delete StreamSelector;
    StreamSelector = nullptr;
  }
}

rtError
rtRemoteInit()
{
  {
    std::lock_guard<std::mutex> lock(gMutex);
    if (gEnv == nullptr)
    {
      gEnv = new rtRemoteEnvironment();
      rtLogDebug("global environment allocated: %p", gEnv);
    }
  }
  return rtRemoteInit(gEnv);
}

rtError
rtRemoteInit(rtRemoteEnvironment* env)
{
  rtError e = RT_FAIL;
  std::lock_guard<std::mutex> lock(gMutex);

  rtLogDebug("initialize environment: %p", env);
  if (!env->Initialized)
  {
    rtLogDebug("environment: %p not initialized, opening server", env);
    e = env->Server->open();
    if (e != RT_OK)
      rtLogError("failed to open rtRemoteServer. %s", rtStrError(e));
  }
  else
  {
    env->RefCount++;
    e = RT_OK;
  }

  if (e == RT_OK)
  {
    rtLogDebug("environment is now initialized: %p", env);
    env->Initialized = true;
  }

  return e;
}

rtError
rtRemoteShutdown()
{
  return rtRemoteShutdown(gEnv);
}

rtError
rtRemoteShutdown(rtRemoteEnvironment* env)
{
  rtError e = RT_FAIL;
  std::lock_guard<std::mutex> lock(gMutex);

  env->RefCount--;
  if (env->RefCount == 0)
  {
    rtLogInfo("environment reference count is zero, deleting");
    env->shutdown();
    if (env == gEnv)
      gEnv = nullptr;
    delete env;
    e = RT_OK;
  }
  else
  {
    rtLogInfo("environment reference count is non-zero. %u", gEnv->RefCount);
    e = RT_OK;
  }

  return e;
}

rtError
rtRemoteRegisterObject(char const* id, rtObjectRef const& obj)
{
  if (gEnv == nullptr)
    return RT_FAIL;

  if (id == nullptr)
    return RT_ERROR_INVALID_ARG;

  if (!obj)
    return RT_ERROR_INVALID_ARG;

  return gEnv->Server->registerObject(id, obj);
}

rtError
rtRemoteLocateObject(char const* id, rtObjectRef& obj)
{
  if (gEnv == nullptr)
  {
    rtLogError("context is null");
    return RT_FAIL;
  }

  if (id == nullptr)
  {
    rtLogError("invalid id (null)");
    return RT_ERROR_INVALID_ARG;
  }

  return gEnv->Server->findObject(id, obj, 3000);
}

rtError
rtRemoteRuncOnce(uint32_t /*timeout*/)
{
  return RT_OK;
}

rtError
rtRemoteRun(uint32_t /*timeout*/)
{
  return RT_OK;
}

