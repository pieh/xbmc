#include "IMediaImport.h"
#include "dialogs/GUIDialogExtendedProgressBar.h"

using namespace MediaImport;

IMediaImport::IMediaImport(const std::string& sourceID, const std::string& friendlyName)
      : m_sourceID(sourceID)
      , m_friendlyName(friendlyName)
      , m_enabled(true)
{
}

const std::string& IMediaImport::GetSourceID() const
{
  return m_sourceID;
}

const std::string& IMediaImport::GetFriendlyName() const
{
  return m_friendlyName;
}

bool IMediaImport::IsDisabled() const
{
  return m_enabled;
}

void IMediaImport::Disable()
{
  m_enabled = false;
}

CMediaImportJob::CMediaImportJob(MediaImportPtr& import, CGUIDialogProgressBarHandle* handle)
  : m_import(import)
  , m_handle(handle)
{
}

CMediaImportJob::~CMediaImportJob()
{
  if (m_handle)
    m_handle->MarkFinished();
}

bool CMediaImportJob::DoWork()
{
  return m_import->DoImport(this);
}

CGUIDialogProgressBarHandle* CMediaImportJob::ProgressBarHandle() const
{
  return m_handle;
}

void CMediaImportJob::AddItem(CONTENT_TYPE contentType, const CFileItemPtr& item)
{
  m_importedMedia[contentType].push_back(item);
}

void CMediaImportJob::SetItems(CONTENT_TYPE contentType, const std::vector<CFileItemPtr>& items)
{
  m_importedMedia[contentType] = items;
}

void CMediaImportJob::GetImportedMedia(CONTENT_TYPE contentType, CFileItemList& list) const
{
  list.SetFastLookup(true);
  std::map<CONTENT_TYPE,std::vector<CFileItemPtr>>::const_iterator it = m_importedMedia.find(contentType);
  if (it != m_importedMedia.end())
  {
    for (std::vector<CFileItemPtr>::const_iterator it2 = it->second.begin() ; it2 != it->second.end() ; it2++)
      list.Add(*it2);
  }
}

MediaImportPtr CMediaImportJob::GetImport() const
{
  return m_import;
}
