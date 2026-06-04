#include "markerManager.h"

WVR_MarkerObserverState dsStatus;

MarkerManager::MarkerManager()
{
    log_count = 0;
}

MarkerManager::~MarkerManager()
{
}

bool MarkerManager::startMarker()
{
    if (WVR_StartMarker() != WVR_Success)
        LOGE("markerAPI WVR_StartMarker fail");
    if (WVR_StartMarkerObserver(WVR_MarkerObserverTarget_Aruco) != WVR_Success)
        LOGE("WVR_StartMarkerObserver fail");

    if (!tryTrackingTrackableMarker())
    {
        LOGI("Not regist any trackable marker. Start tracking now");
        tryDetectingArucoMarker();
    }
    return true;
}

bool MarkerManager::startTrackingAllTrackableMarker()
{
    refreshTrackableMarker();
    if (trackableMarkerData.size() == 0)
        return false;

    // start tracking all trackable aruco markers
    for (int a = 0; a < trackableMarkerData.size(); a++)
    {
        // trackable marker status will be WVR_MarkerTrackingState_Tracked or WVR_MarkerTrackingState_Pause
        if (WVR_StartTrackableMarkerTracking(trackableMarkerData[a].uuid) != WVR_Success)
            LOGE("markerAPI WVR_StartTrackableMarkerTracking marker %d fail", a);
    }
    return true;
}

bool MarkerManager::stopTrackingAllTrackableMarker()
{
    //  stop tracking all trackable markers
    for (int a = 0; a < trackableIds.size(); a++)
    {
        // trackable marker status will be WVR_MarkerTrackingState_Stopped
        LOGI("stop tracking all trackable marker at [%d]", a);
        if (WVR_StopTrackableMarkerTracking(trackableIds[a]) != WVR_Success)
            LOGE("markerAPI WVR_StopTrackableMarkerTracking fail");
    }
    refreshTrackableMarker();
    return true;
}

bool MarkerManager::stopTrackingTrackableMarker(int selectIndex)
{
    if (selectIndex >= 0)
    {
        LOGI("stop trackable marker at [%d]", selectIndex);
        WVR_StopTrackableMarkerTracking(trackableIds[selectIndex]);
    }
    else
    {
        LOGI("no trackable marker can stop");
    }
    refreshTrackableMarker();
    return false;
}

bool MarkerManager::stopMarker()
{
    if (WVR_StopMarkerObserver(WVR_MarkerObserverTarget_Aruco) != WVR_Success)
        LOGE("markerAPI WVR_StopMarkerObserver fail");
    WVR_StopMarker();
    return true;
}

bool MarkerManager::tryDetectingArucoMarker()
{
    WVR_MarkerObserverState state;
    WVR_GetMarkerObserverState(WVR_MarkerObserverTarget_Aruco, &state);

    switch (state)
    {
    case WVR_MarkerObserverState_Detecting:
        LOGI("Observe state already WVR_MarkerObserverState_Detecting");
        break;
    case WVR_MarkerObserverState_Tracking:
        stopTrackingAllTrackableMarker();
        LOGI("Observe state WVR_MarkerObserverState_Tracking -> WVR_MarkerObserverState_Detecting");
        if (WVR_StartMarkerDetection(WVR_MarkerObserverTarget_Aruco))
            LOGE("marker init: markerAPI WVR_StartMarkerDetection fail");
        break;
    case WVR_MarkerObserverState_Idle:
        LOGI("Observe state WVR_MarkerObserverState_Idle -> WVR_MarkerObserverState_Detecting");
        if (WVR_StartMarkerDetection(WVR_MarkerObserverTarget_Aruco))
            LOGE("marker init: markerAPI WVR_StartMarkerDetection fail");
        break;
    }

    WVR_GetMarkerObserverState(WVR_MarkerObserverTarget_Aruco, &state);
    switch (state)
    {
    case WVR_MarkerObserverState_Detecting:
        LOGI("Currentss observe state = WVR_MarkerObserverState_Detecting");
        break;
    case WVR_MarkerObserverState_Tracking:
        LOGI("Currentss observe = WVR_MarkerObserverState_Tracking");
        break;
    case WVR_MarkerObserverState_Idle:
        LOGI("Currentss observe = WVR_MarkerObserverState_Idle");
        break;
    }
    return true;
}

bool MarkerManager::tryTrackingTrackableMarker()
{
    WVR_MarkerObserverState state;
    WVR_GetMarkerObserverState(WVR_MarkerObserverTarget_Aruco, &state);

    if (state == WVR_MarkerObserverState_Detecting)
        WVR_StopMarkerDetection(WVR_MarkerObserverTarget_Aruco);

    switch (state)
    {
    case WVR_MarkerObserverState_Detecting:
        LOGI("Observe state WVR_MarkerObserverState_Detecting -> WVR_MarkerObserverState_Tracking");
        return startTrackingAllTrackableMarker();
        break;
    case WVR_MarkerObserverState_Tracking:
        LOGI("Observe state already WVR_MarkerObserverState_Tracking");
        break;
    case WVR_MarkerObserverState_Idle:
        LOGI("Observe state WVR_MarkerObserverState_Idle -> WVR_MarkerObserverState_Tracking");
        return startTrackingAllTrackableMarker();
        break;
    }
    return true;
}

WVR_MarkerObserverState MarkerManager::getObserveState()
{
    WVR_MarkerObserverState state;
    WVR_GetMarkerObserverState(WVR_MarkerObserverTarget_Aruco, &state);
    log_count = log_count % 700 + 10;
    if (log_count == 600)
    {
        switch (state)
        {
        case WVR_MarkerObserverState_Detecting:
            LOGI("Current observe state = WVR_MarkerObserverState_Detecting");
            break;
        case WVR_MarkerObserverState_Tracking:
            LOGI("Current observe = WVR_MarkerObserverState_Tracking");
            break;
        case WVR_MarkerObserverState_Idle:
            LOGI("Current observe = WVR_MarkerObserverState_Idle");
            break;
        }
    }
    return state;
}

void MarkerManager::getDetectingData()
{
    //  get plane size
    static uint32_t lastDetectSize = -99;
    uint32_t firstCallSize = 0;
    detectingArucoData.clear();
    WVR_GetArucoMarkers(firstCallSize, &firstCallSize, WVR_PoseOriginModel_OriginOnHead, nullptr);
    detectingArucoData.resize(firstCallSize);

    //  get data to render plane ( WVR_MarkerTrackingState_Detected->white ; WVR_MarkerTrackingState_Stopped->green )
    uint32_t secondCallSize = 0;
    WVR_GetArucoMarkers(firstCallSize, &secondCallSize, WVR_PoseOriginModel_OriginOnHead, detectingArucoData.data());

    // prevent something change between first call to second call
    if (firstCallSize > secondCallSize)
        detectingArucoData.resize(secondCallSize);

    if (secondCallSize != lastDetectSize)
    {
        LOGI("WVR_GetArucoMarkers firstCallSize=%d, secondCallSize=%d", firstCallSize, secondCallSize);
        lastDetectSize = secondCallSize;
    }
}

void MarkerManager::filterTrackableAruco()
{
    //  get trackable aruco marker data to render
    trackableMarkerData.clear();
    WVR_TrackableMarkerState tempState;
    WVR_ArucoMarkerData tempData;
    for (int a = 0; a < trackableIds.size(); a++)
    {
        WVR_GetTrackableMarkerState(trackableIds[a], WVR_PoseOriginModel_OriginOnHead, &tempState);
        // only render aruco marker
        if (tempState.target == WVR_MarkerObserverTarget_Aruco)
        {
            WVR_GetArucoMarkerData(trackableIds[a], &tempData);
            trackableMarkerData.push_back(mTrackableArucoData{
                .uuid = trackableIds[a],
                .markerState = tempState,
                .arucoData = tempData});
        }
    }
}

void MarkerManager::refreshTrackableMarker()
{
    uint32_t firstCallSize = 0;
    trackableIds.clear();
    WVR_EnumerateTrackableMarkers(WVR_MarkerObserverTarget_Aruco, firstCallSize, &firstCallSize, nullptr);
    trackableIds.resize(firstCallSize);

    uint32_t secondCallSize = 0;
    WVR_EnumerateTrackableMarkers(WVR_MarkerObserverTarget_Aruco, firstCallSize, &secondCallSize, trackableIds.data());
    LOGI("WVR_EnumerateTrackableMarkers get firstCallSize()=%d, secondCallSize()=%d", firstCallSize, secondCallSize);

    // prevent something change between first call to second call
    if (firstCallSize > secondCallSize)
        trackableIds.resize(secondCallSize);

    filterTrackableAruco();
}

void MarkerManager::getTrackingData()
{
    static uint32_t lastTrackingSize = -99;
    if (trackableMarkerData.size() != lastTrackingSize)
    {
        LOGI("trackableMarkerData size=%d", trackableMarkerData.size());
        lastTrackingSize = trackableMarkerData.size();
    }

    for (int a = 0; a < trackableMarkerData.size(); a++)
    {
        WVR_GetTrackableMarkerState(trackableMarkerData[a].uuid, WVR_PoseOriginModel_OriginOnHead, &trackableMarkerData[a].markerState);
        WVR_GetArucoMarkerData(trackableMarkerData[a].uuid, &trackableMarkerData[a].arucoData);
    }
}

bool MarkerManager::deleteAllTrackableMarker()
{
    LOGI("try destroy all");
    if(WVR_ClearTrackableMarkers()!=WVR_Success)
        LOGI("try destroy fail");

    // refresh data
    getDetectingData();
    return true;
}

bool MarkerManager::deleteTrackableMarker(int selectIndex)
{
    LOGI("try destroy one");
    if (selectIndex >= 0)
    {
        LOGI("destroy trackable marker at [%d]", selectIndex);
        WVR_DestroyTrackableMarker(detectingArucoData[selectIndex].uuid);
    }
    else
    {
        LOGI("no trackable marker can be destroy");
    }
    // refresh data
    getDetectingData();
    return true;
}

bool MarkerManager::addTrackableMarker(int selectIndex)
{
    if (selectIndex >= 0)
    {
        LOGI("add trackable marker at [%d]", selectIndex);
        char name[] = "helloMarker\0";
        WVR_MarkerName markerName;
        strncpy(markerName.name, name, sizeof(name) / sizeof(char));
        WVR_TrackableMarkerCreateInfo createInfo{detectingArucoData[selectIndex].uuid, markerName};
        // All created trackableMarker WVR_MarkerTrackingState status will be WVR_MarkerTrackingState_Stopped, until WVR_StartTrackableMarkerTracking being call.
        WVR_CreateTrackableMarker(&createInfo);
    }
    else
    {
        LOGI("no aruco can add as trackable marker");
    }
    return true;
}