#include "MediaImporter.h"

#include "threads/SingleLock.h"
#include "video/VideoDatabase.h"
#include "music/MusicDatabase.h"
#include "utils/JobManager.h"
#include "guilib/GUIWindowManager.h"
#include "dialogs/GUIDialogExtendedProgressBar.h"
#include "utils/StringUtils.h"
#include "interfaces/AnnouncementManager.h"

using namespace MediaImport;

CMediaImporter::CMediaImporter()
{
}

CMediaImporter::~CMediaImporter()
{
  CSingleLock lock(m_importersLock);
  for (std::vector<MediaImportPtr>::iterator it = m_importers.begin() ; it != m_importers.end() ; it++)
    CleanupImportSource(*it);
  m_importers.clear();
}

CMediaImporter& CMediaImporter::Get()
{
  static CMediaImporter instance;
  return instance;
}

void CMediaImporter::RegisterImportSource(MediaImportPtr importer)
{
  CSingleLock lock(m_importersLock);

  if (FindImportSource(importer->GetSourceID()) == m_importers.end())
    m_importers.push_back(importer);

  TriggerImport(importer);
}

void CMediaImporter::TriggerImport(MediaImportPtr importer)
{
  CSingleLock lock(m_importJobsLock);
  if (FindImportJob(importer))
    return; // job is already running

  CGUIDialogExtendedProgressBar* dialog = (CGUIDialogExtendedProgressBar*)g_windowManager.GetWindow(WINDOW_DIALOG_EXT_PROGRESS);
  CGUIDialogProgressBarHandle *handle = dialog->GetHandle("Fetching: " + importer->GetFriendlyName());
  

  CMediaImportJob* job = new CMediaImportJob(importer, handle);
  int jobID = CJobManager::GetInstance().AddJob(job, this);

  if (jobID)
    m_importJobs[jobID] = importer;
}

bool CMediaImporter::UnregisterImportSource(std::vector<MediaImportPtr>::iterator& it)
{
  MediaImportPtr importer = *it;
  CSingleLock lock(m_importersLock);
  if (it != m_importers.end())
  {
    it =  m_importers.erase(it);
    lock.Leave();
    CleanupImportSource(importer);
    return true;
  }
  else
    return false;
}

unsigned int CMediaImporter::FindImportJob(MediaImportPtr& importer)
{
  CSingleLock lock(m_importJobsLock);
  for (std::map<unsigned int,MediaImportPtr>::iterator it = m_importJobs.begin() ; it != m_importJobs.end() ; it++)
  {
    if (it->second == importer)
      return it->first;
  }
  return 0;
}

void CMediaImporter::CancelImportJob(MediaImportPtr importer)
{
  CSingleLock lock(m_importJobsLock);
  unsigned int jobID = FindImportJob(importer);
  if (jobID)
  {
    CJobManager::GetInstance().CancelJob(jobID);
    m_importJobs.erase(jobID);
  }
}

std::vector<MediaImportPtr>::iterator CMediaImporter::FindImportSource(const std::string& sourceID)
{
  CSingleLock lock(m_importersLock);
  for (std::vector<MediaImportPtr>::iterator it = m_importers.begin() ; it != m_importers.end() ; it++)
  {
    if ((*it)->GetSourceID() == sourceID)
      return it;
  }
  return m_importers.end();
}

bool CMediaImporter::UnregisterImportSource(const std::string& sourceID)
{
  CSingleLock lock(m_importersLock);
  std::vector<MediaImportPtr>::iterator it = FindImportSource(sourceID);
  if (it != m_importers.end())
  {
    UnregisterImportSource(it);
    return true;
  }
  else
    return false;
}

void CMediaImporter::CleanupImportSource(MediaImportPtr importer)
{
  CancelImportJob(importer);
  importer->Disable();

  // we can't trust what importer would say it provides (music and/or videos):
  // with new update addon might stop saying that it provides videos and so we end up with old media in video database
  // we disable media in both databases - it might be cleaned on next import
  {
    CVideoDatabase db;
    if (db.Open())
    {
      db.SetEnabledForRemoteItems(importer->GetSourceID(), false);
      db.Close();
    }
  }
}

static void SetDetailsForFile(CFileItemPtr pItem, CVideoDatabase& videodb, bool reset)
{
  if (reset)
  {
    // clean resume bookmark
    videodb.DeleteResumeBookMark(pItem->GetPath());
  }

  if (pItem->GetVideoInfoTag()->m_resumePoint.IsPartWay())
    videodb.AddBookMarkToFile(pItem->GetPath(), pItem->GetVideoInfoTag()->m_resumePoint, CBookmark::RESUME);

  if (pItem->GetVideoInfoTag()->m_playCount)
    videodb.SetPlayCount(*pItem, pItem->GetVideoInfoTag()->m_playCount, pItem->GetVideoInfoTag()->m_lastPlayed);
}


static void ProcessMovies(CMediaImportJob* importJob, CVideoDatabase& videodb)
{
  MediaImportPtr importer = importJob->GetImport();
  importJob->ProgressBarHandle()->SetTitle("Importing movies: " + importer->GetFriendlyName());
  importJob->ProgressBarHandle()->SetProgress(0, 1);

  CFileItemList storedItems, newItems;
  importJob->GetImportedMedia(MediaImport::CONTENT_MOVIES, newItems);
  videodb.GetMoviesByWhere("videodb://movies/titles/", StringUtils::Format("strSource = '%s' AND enabled = 0", importer->GetSourceID().c_str()), storedItems, SortDescription(), true);

  int progress = 0;
  int total = storedItems.Size() + newItems.Size();
  for (int i = 0; i < storedItems.Size() ; i++)
  {
    CFileItemPtr& oldItem = storedItems[i];
    CFileItemPtr pItem = newItems.Get(oldItem->GetVideoInfoTag()->m_strFileNameAndPath);

    if (!pItem) // delete items that are not in newMovieItems
      videodb.DeleteMovie(oldItem->GetVideoInfoTag()->m_iDbId);
    else // item is in both lists
    {
      newItems.Remove(pItem.get()); // we want to get rid of items we already have from the new movies list
      total--;

      // check if we need to update (db writing is expensive)
      if (*(oldItem->GetVideoInfoTag()) != *(pItem->GetVideoInfoTag()))
      {
        videodb.SetDetailsForMovie(pItem->GetPath(), *(pItem->GetVideoInfoTag()), pItem->GetArt(), pItem->GetVideoInfoTag()->m_iDbId);
        SetDetailsForFile(pItem, videodb, true);
      }

    }

    importJob->ProgressBarHandle()->SetProgress(progress++, total);
  }

  for (int i = 0; i < newItems.Size() ; i++)
  {
    CFileItemPtr& pItem = newItems[i];
    videodb.SetDetailsForMovie(pItem->GetPath(), *(pItem->GetVideoInfoTag()), pItem->GetArt());
    SetDetailsForFile(pItem, videodb, false);

    importJob->ProgressBarHandle()->SetProgress(progress++, total);
  }
}

static bool IsSameTVShow(CVideoInfoTag& left, CVideoInfoTag& right)
{
  return left.m_strShowTitle == right.m_strShowTitle
      && left.m_iYear        == right.m_iYear;
}

static void ProcessTVShowsAndEpisodes(CMediaImportJob* importJob, CVideoDatabase& videodb)
{
  MediaImportPtr importer = importJob->GetImport();

  std::map<std::string,int> tvshowMap;
  std::vector<CFileItemPtr> notAvailableTvShows;

  {
    importJob->ProgressBarHandle()->SetTitle("Importing TV shows: " + importer->GetFriendlyName());
    importJob->ProgressBarHandle()->SetProgress(0, 1);

    CFileItemList storedItems, newItems;
    importJob->GetImportedMedia(MediaImport::CONTENT_TVSHOWS, newItems);
    videodb.GetTvShowsByWhere("videodb://tvshows/titles/", StringUtils::Format("strSource = '%s'", importer->GetSourceID().c_str()), storedItems);

    int progress = 0;
    int total = storedItems.Size() + newItems.Size();

    for (int i = 0; i < storedItems.Size() ; i++)
    {
      CFileItemPtr& oldItem = storedItems[i];

      for (int j = 0; j < newItems.Size() ; j++)
      {
        CFileItemPtr& newItem = newItems[j];
        if (IsSameTVShow(*oldItem->GetVideoInfoTag(), *newItem->GetVideoInfoTag()))
        {
          newItems.Remove(j);
          total--;
          tvshowMap[newItem->GetVideoInfoTag()->m_strShowTitle] = oldItem->GetVideoInfoTag()->m_iDbId;

          importJob->ProgressBarHandle()->SetProgress(progress++, total);
          // TO-DO check if we can update meta
          break;
        }

      }

      // not found push on the list
      notAvailableTvShows.push_back(oldItem);
      importJob->ProgressBarHandle()->SetProgress(progress++, total);
    }

    for (int i = 0; i < newItems.Size() ; i++)
    {
      CFileItemPtr& pItem = newItems[i];

      std::map<int, std::map<std::string, std::string> > seasonArt;
      int tvshowID = videodb.SetDetailsForTvShow(pItem->GetPath(), *pItem->GetVideoInfoTag(), pItem->GetArt(), seasonArt);

      if (tvshowID > -1)
      {
        tvshowMap[pItem->GetVideoInfoTag()->m_strShowTitle] = tvshowID;
      }
      importJob->ProgressBarHandle()->SetProgress(progress++, total);
    }
  }

  // group episodes into tv shows
  {
    importJob->ProgressBarHandle()->SetTitle("Importing episodes: " + importer->GetFriendlyName());
    importJob->ProgressBarHandle()->SetProgress(0, 1);

    CFileItemList storedItems, newItems;
    importJob->GetImportedMedia(MediaImport::CONTENT_EPISODES, newItems);
    videodb.GetEpisodesByWhere("videodb://tvshows/titles/", StringUtils::Format("strSource = '%s'", importer->GetSourceID().c_str()), storedItems);

    int progress = 0;
    int total = storedItems.Size() + newItems.Size();
    for (int i = 0; i < storedItems.Size() ; i++)
    {
      CFileItemPtr& oldItem = storedItems[i];
      CFileItemPtr pItem = newItems.Get(oldItem->GetVideoInfoTag()->m_strFileNameAndPath);

      if (!pItem) // delete items that are not in newMovieItems
        videodb.DeleteEpisode(oldItem->GetVideoInfoTag()->m_iDbId);
      else // item is in both lists
      {
        newItems.Remove(pItem.get()); // we want to get rid of items we already have from the new movies list
        total--;
        // check if we need to update (db writing is expensive)
        if (*(oldItem->GetVideoInfoTag()) != *(pItem->GetVideoInfoTag()))
        {
          videodb.SetDetailsForEpisode(pItem->GetPath(), *(pItem->GetVideoInfoTag()), pItem->GetArt(), -1, oldItem->GetVideoInfoTag()->m_iDbId);
          SetDetailsForFile(pItem, videodb, true);
        }
        importJob->ProgressBarHandle()->SetProgress(progress++, total);
      }
    }

    for (int i = 0; i < newItems.Size() ; i++)
    {
      CFileItemPtr& pItem = newItems[i];

      // determine idTvShow
      std::map<std::string,int>::iterator it = tvshowMap.find(pItem->GetVideoInfoTag()->m_strShowTitle);
      if (it != tvshowMap.end())
      {
        int idTvShow = it->second;
        videodb.SetDetailsForEpisode(pItem->GetPath(), *(pItem->GetVideoInfoTag()), pItem->GetArt(), idTvShow);
        SetDetailsForFile(pItem, videodb, false);
      }
      importJob->ProgressBarHandle()->SetProgress(progress++, total);
    }
  }
}

void CMediaImporter::OnJobComplete(unsigned int jobID, bool success, CJob *job)
{
  CMediaImportJob* importJob = (CMediaImportJob*)job;

  CSingleLock lock(m_importJobsLock);
  MediaImportPtr importer = m_importJobs[jobID];

  if (!importer)
    return;

  m_importJobs.erase(jobID);

  if (!success)
    return;

  CVideoDatabase videodb;
  if (!videodb.Open())
    return;

  // do only needed operations:
  // - remove item IF it is not found in new listing
  // - update metadata IF it has changed
  // - insert new media IF it wasnt stored yet

  ProcessMovies(importJob, videodb);
  ProcessTVShowsAndEpisodes(importJob, videodb);

  // enable all items
  videodb.SetEnabledForRemoteItems(importer->GetSourceID(), true);
  videodb.Close();

  ANNOUNCEMENT::CAnnouncementManager::Announce(ANNOUNCEMENT::VideoLibrary, "xbmc", "OnScanFinished");
  ANNOUNCEMENT::CAnnouncementManager::Announce(ANNOUNCEMENT::AudioLibrary, "xbmc", "OnScanFinished");
}

void CMediaImporter::OnJobProgress(unsigned int jobID, unsigned int progress, unsigned int total, const CJob *job)
{
  CMediaImportJob* importJob = (CMediaImportJob*)job;
  importJob->ProgressBarHandle()->SetProgress(progress, total);
}
