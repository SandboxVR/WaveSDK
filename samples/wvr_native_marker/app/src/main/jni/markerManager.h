#ifndef WVR_NATIVE_MARKER_MARKERMANAGER_H
#define WVR_NATIVE_MARKER_MARKERMANAGER_H
#include <wvr/wvr_scene.h>
#include <wvr/wvr_marker.h>
#include <log.h>
#include <wvr/wvr_device.h>
#include <vector>
#include <Cube.h>
#include <Plane.h>

typedef struct mTrackableArucoData
{
    WVR_Uuid uuid;
    WVR_TrackableMarkerState markerState;
    WVR_ArucoMarkerData arucoData;
} mTrackableArucoData;

class MarkerManager
{
private:
    int log_count = 0;
    std::vector<WVR_Uuid> trackableIds;
    void filterTrackableAruco();

public:
    std::vector<WVR_ArucoMarker> detectingArucoData;
    std::vector<mTrackableArucoData> trackableMarkerData;
    MarkerManager();
    ~MarkerManager();

    bool startMarker();
    bool stopMarker();

    WVR_MarkerObserverState getObserveState();

    bool tryTrackingTrackableMarker();
    bool startTrackingAllTrackableMarker();
    bool stopTrackingAllTrackableMarker();
    bool stopTrackingTrackableMarker(int selectIndex);
    void getTrackingData();
    void refreshTrackableMarker();

    bool tryDetectingArucoMarker();
    void getDetectingData();
    bool addTrackableMarker(int selectIndex);
    bool deleteTrackableMarker(int selectIndex);
    bool deleteAllTrackableMarker();
};

#endif // WVR_NATIVE_MARKER_MARKERMANAGER_H
