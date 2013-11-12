#ifndef TORCPLUGIN_H
#define TORCPLUGIN_H

// Torc
#include "torccompat.h"
#include "torclibrary.h"

typedef bool (APIENTRY * TORC_LOAD_PLUGIN)   (const char*);
typedef bool (APIENTRY * TORC_UNLOAD_PLUGIN) (void);

class TorcPlugin
{
  public:
    TorcPlugin();
    ~TorcPlugin();

  private:
    void Load (const QString &Directory);

  private:
    QList<TorcLibrary*> m_loadedPlugins;
};
#endif // TORCPLUGIN_H
