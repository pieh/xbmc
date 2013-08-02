#pragma once

#include <string>
#include "utils/Job.h"
#include "boost/shared_ptr.hpp"
#include "FileItem.h"

class CGUIDialogProgressBarHandle;

namespace MediaImport
{
  typedef enum
  {
    CONTENT_MOVIES,
    CONTENT_TVSHOWS,
    CONTENT_MUSICVIDEOS,
    CONTENT_EPISODES,
    CONTENT_MOVIE_SETS,
    CONTENT_ALBUMS,
    CONTENT_ARTISTS,
    CONTENT_SONGS,
    CONTENT_NONE
  } CONTENT_TYPE;

  class CMediaImportJob;

  class IMediaImport
  {
  protected:
    IMediaImport(const std::string& sourceID, const std::string& friendlyName);

    std::string m_sourceID;
    std::string m_friendlyName;
    
    bool m_enabled;
  public:
    const std::string& GetSourceID() const;
    const std::string& GetFriendlyName() const;
    bool IsDisabled() const;
    void Disable();

    // CJob passed to check if we need to cancel import and to report progress
    virtual bool DoImport(CMediaImportJob* job) = 0;
  };

  typedef boost::shared_ptr<IMediaImport> MediaImportPtr;

  // job is deleted after work is done, so we need proxy job to trigger IMediaImport::DoImport
  class CMediaImportJob : public CJob
  {
  protected:
    MediaImportPtr m_import;
    CGUIDialogProgressBarHandle* m_handle;
    std::map<CONTENT_TYPE,std::vector<CFileItemPtr>> m_importedMedia;
  public:
    CMediaImportJob(MediaImportPtr& import, CGUIDialogProgressBarHandle* handle);
    ~CMediaImportJob();

    // for workers
    void AddItem(CONTENT_TYPE contentType, const CFileItemPtr& item);
    void SetItems(CONTENT_TYPE contentType, const std::vector<CFileItemPtr>& items);

    // for manager
    void GetImportedMedia(CONTENT_TYPE contentType, CFileItemList& items) const;
    MediaImportPtr GetImport() const;

    virtual bool DoWork();
    CGUIDialogProgressBarHandle* ProgressBarHandle() const;
    virtual const char *GetType() const { return "MediaImportJob"; };
  };
}