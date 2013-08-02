#include "UPnPImport.h"
#include "Platinum.h"
#include "utils/log.h"
#include "URL.h"
#include "utils/URIUtils.h"
#include "video/VideoInfoTag.h"
#include "music/tags/MusicInfoTag.h"

using namespace UPNP;
using namespace MediaImport;

#define UPNP_ROOT_CONTAINER_ID "0"

static CFileItemPtr ConstructItem(PLT_DeviceDataReference& device, PLT_MediaObject* object)
{
  CFileItemPtr pItem = BuildObject(object);
  CStdString id;
  if (object->m_ReferenceID.IsEmpty())
      id = (const char*) object->m_ObjectID;
  else
      id = (const char*) object->m_ReferenceID;

  CURL::Encode(id);
  URIUtils::AddSlashAtEnd(id);
  pItem->SetPath(CStdString((const char*) "upnp://" + device->GetUUID() + "/" + id.c_str()));

  pItem->GetVideoInfoTag()->m_strPath = pItem->GetVideoInfoTag()->m_strFileNameAndPath = pItem->GetPath();
  return pItem;
}

static void ConstructList(PLT_DeviceDataReference& device, PLT_MediaObjectListReference& list, std::vector<CFileItemPtr>& items)
{
  if (list.IsNull())
    return;

  for (PLT_MediaObjectList::Iterator entry = list->GetFirstItem() ; entry ; entry++)
  {
    CFileItemPtr item(ConstructItem(device, *entry));
    if (item)
      items.push_back(item);
  }
}

static bool Search(PLT_DeviceDataReference& device, const std::string& search_criteria, std::vector<CFileItemPtr>& items)
{
  PLT_MediaObjectListReference list;
  if (CUPnP::GetInstance()->m_MediaBrowser->SearchSync(device, UPNP_ROOT_CONTAINER_ID, search_criteria.c_str(), list, 0, 0) == NPT_SUCCESS)
  {
    ConstructList(device, list, items);
    return true;
  }
  return false;
}

static void DoImportVideos(CMediaImportJob* job, PLT_DeviceDataReference& device)
{
  { // import movies
    if (job->ShouldCancel(0, 3))
      return;

    std::vector<CFileItemPtr> items;
    if (Search(device, "object.item.videoItem.movie", items))
      job->SetItems(CONTENT_MOVIES, items);
  }

  { // import tvshows
    if (job->ShouldCancel(1, 3))
      return;

    std::vector<CFileItemPtr> items;
    if (Search(device, "object.container.album.videoAlbum", items))
    {
      std::vector<CFileItemPtr> tvshowItems;
      for (std::vector<CFileItemPtr>::iterator it = items.begin() ; it != items.end() ; it++)
      { // discard video albums that are NOT tv shows
        if ((*it)->HasVideoInfoTag() && ((*it)->GetVideoInfoTag()->m_type == "tvshow" || (*it)->GetVideoInfoTag()->m_type == "season"))
          tvshowItems.push_back(*it);
      }
      job->SetItems(CONTENT_TVSHOWS, tvshowItems);
    }
  }

  { // import episodes
    if (job->ShouldCancel(2, 3))
      return;

    std::vector<CFileItemPtr> items;
    if (Search(device, "object.item.videoItem.videoBroadcast", items))
      job->SetItems(CONTENT_EPISODES, items);
  }
}

static void DoImportMusic(CMediaImportJob* job, PLT_DeviceDataReference& device)
{
  /*
  if (job->ShouldCancel(0, 10))
    return;

  std::vector<CFileItemPtr> songItems;
  bool SongSearchStatus = Search(device, "object.item.audioItem", songItems);

  if (job->ShouldCancel(0, 10))
    return;

  std::vector<CFileItemPtr> albumItems;
  bool AlbumSearchStatus = Search(device, "object.container.album.musicAlbum", albumItems);

  if (job->ShouldCancel(0, 10))
    return;

  std::vector<CFileItemPtr> aritstItems;
  bool ArtistSearchStatus = Search(device, "object.container.person.musicArtist", aritstItems);

  if (!songItems.empty())
    job->SetItems(CONTENT_SONGS, songItems);

  if (!albumItems.empty())
    job->SetItems(CONTENT_ALBUMS, albumItems);

  if (!aritstItems.empty())
    job->SetItems(CONTENT_ARTISTS, aritstItems);
    */
}

CUPnPImport::CUPnPImport(const std::string& deviceUUID, const std::string& friendlyName)
  : IMediaImport(StringUtils::Format("upnp://%s", deviceUUID.c_str()), friendlyName)
  , m_deviceUUID(deviceUUID)
{
}

bool CUPnPImport::DoImport(CMediaImportJob* job)
{
  PLT_DeviceDataReference device;
  if (!CUPnP::GetInstance()->m_MediaBrowser->FindServer(m_deviceUUID.c_str(), device) == NPT_SUCCESS)
    return false;


  if (m_friendlyName.find("GSoC") == std::string::npos)
    return false;

  DoImportVideos(job, device);
  DoImportMusic(job, device);

  return true;
}
