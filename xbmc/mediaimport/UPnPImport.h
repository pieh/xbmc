#pragma once

#include "IMediaImport.h"
#include "utils/StringUtils.h"
#include "network/upnp/UPnP.h"
#include "network/upnp/UPnPInternal.h"

using namespace MediaImport;
using namespace UPNP;

class CUPnPImport : public MediaImport::IMediaImport
{
protected:
  std::string m_deviceUUID;
public:
  CUPnPImport(const std::string& deviceUUID, const std::string& friendlyName);
  virtual bool DoImport(CMediaImportJob* job);
};