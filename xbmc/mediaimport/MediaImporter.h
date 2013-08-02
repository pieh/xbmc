#include "threads/CriticalSection.h"
#include <vector>
#include "IMediaImport.h"
#include "utils/Job.h"

namespace MediaImport
{
  class CMediaImporter : public IJobCallback
  {
  protected:
    CMediaImporter();

    bool UnregisterImportSource(std::vector<MediaImportPtr>::iterator& it);
    void CleanupImportSource(MediaImportPtr importer);
    void TriggerImport(MediaImportPtr importer);
    void CancelImportJob(MediaImportPtr importer);

    std::vector<MediaImportPtr>::iterator FindImportSource(const std::string& sourceID);
    unsigned int FindImportJob(MediaImportPtr& importer);

    CCriticalSection m_importersLock;
    std::vector<MediaImportPtr> m_importers;

    CCriticalSection m_importJobsLock;
    std::map<unsigned int,MediaImportPtr> m_importJobs;
  public:
    ~CMediaImporter();
    static CMediaImporter& Get();

    void RegisterImportSource(MediaImportPtr importer);
    bool UnregisterImportSource(const std::string& sourceID);

    // IJobCallback
    virtual void OnJobComplete(unsigned int jobID, bool success, CJob *job);
    virtual void OnJobProgress(unsigned int jobID, unsigned int progress, unsigned int total, const CJob *job);
  };
};