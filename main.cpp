#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>
#include <iostream>
#include <memory>
#include <string>

const std::string VIDEO_PATH = "/Users/anshumantiwari/Documents/codes/personal/C++/tracking/track.mp4";

typedef class TRACKING_MANAGER
{
public:
    TRACKING_MANAGER()
    {
        tracker = cv::TrackerCSRT::create();
        if (!tracker)
        {
            throw std::runtime_error("Failed to create tracker");
        }
    }

    cv::Ptr<cv::Tracker> getTracker() const { return tracker; }

private:
    cv::Ptr<cv::Tracker> tracker;
} TrackingManager;

typedef struct ROI_STATE
{
    cv::Point2i initial{0, 0};
    cv::Point2i current{0, 0};
    cv::Size finalSize{0, 0};
    bool showRoi{false};
    bool trackerReady{false};
    bool initializeTracker{false};
} RoiState;

typedef class VIDEO_TRACKER
{
private:
    cv::Mat frame;
    ROI_STATE roiState;
    std::unique_ptr<TRACKING_MANAGER> trackerMgr;
    cv::VideoWriter videoOut;

    cv::Rect2d normalizeRoi(cv::Point2i start, cv::Point2i end)
    {
        int x = std::min(start.x, end.x);
        int y = std::min(start.y, end.y);
        int width = std::abs(end.x - start.x);
        int height = std::abs(end.y - start.y);

        x = std::max(0, x);
        y = std::max(0, y);
        width = std::min(width, frame.cols - x);
        height = std::min(height, frame.rows - y);

        return cv::Rect2d(x, y, width, height);
    }

    bool isValidRoi(const cv::Rect2d &roi)
    {
        return roi.width > 0 && roi.height > 0 &&
               roi.x >= 0 && roi.y >= 0 &&
               (roi.x + roi.width) <= frame.cols &&
               (roi.y + roi.height) <= frame.rows;
    }

    void setupVideoOutput(const cv::VideoCapture &cap)
    {
        videoOut.open("video.mp4",
                      cv::VideoWriter::fourcc('X', '2', '6', '4'),
                      cap.get(cv::CAP_PROP_FPS),
                      cv::Size(1024, 800),
                      true);
        if (!videoOut.isOpened())
        {
            throw std::runtime_error("Failed to open video output");
        }
    }

public:
    VIDEO_TRACKER() : trackerMgr(std::make_unique<TRACKING_MANAGER>()) {}

    static void mouseCallback(int event, int x, int y, int flags, void *userdata)
    {
        VIDEO_TRACKER *tracker = static_cast<VIDEO_TRACKER *>(userdata);
        tracker->handleMouseEvent(event, x, y);
    }

    void processVideo()
    {
        cv::VideoCapture cap(VIDEO_PATH);
        if (!cap.isOpened())
        {
            throw std::runtime_error("Failed to open video capture");
        }

        setupVideoOutput(cap);

        cv::namedWindow("Video", cv::WINDOW_AUTOSIZE);
        cv::setMouseCallback("Video", mouseCallback, this);

        while (true)
        {
            if (!cap.read(frame))
            {
                std::cout << "Video capture failed" << std::endl;
                break;
            }

            cv::resize(frame, frame, cv::Size(1024, 800));
            processFrame();

            videoOut << frame;
            cv::imshow("Video", frame);

            if (cv::waitKey(20) >= 0)
                break;
        }
    }

private:
    void handleMouseEvent(int event, int x, int y)
    {
        switch (event)
        {
        case cv::EVENT_LBUTTONDOWN:
            roiState.initial = cv::Point2i(x, y);
            roiState.showRoi = true;
            std::cout << "Left button pressed at: " << x << "," << y << std::endl;
            break;

        case cv::EVENT_MOUSEMOVE:
            roiState.current = cv::Point2i(x, y);
            break;

        case cv::EVENT_LBUTTONUP:
        {
            cv::Rect2d roi = normalizeRoi(roiState.initial, cv::Point2i(x, y));
            if (isValidRoi(roi))
            {
                roiState.finalSize = cv::Size(roi.width, roi.height);
                roiState.initial = cv::Point2i(roi.x, roi.y);
                roiState.initializeTracker = true;
                roiState.showRoi = false;
                std::cout << "Left button released" << std::endl;
            }
            else
            {
                std::cout << "Invalid ROI selected" << std::endl;
                roiState.showRoi = false;
            }
            break;
        }

        case cv::EVENT_RBUTTONDOWN:
            std::cout << "Right button pressed" << std::endl;
            break;
        }
    }

    void processFrame()
    {

        if (roiState.showRoi)
        {
            cv::Rect2d roi = normalizeRoi(roiState.initial, roiState.current);
            if (isValidRoi(roi))
            {
                cv::rectangle(frame, roi, cv::Scalar(255, 255, 255), 4);
            }
        }

        if (roiState.initializeTracker)
        {
            cv::Rect2d roi(roiState.initial, roiState.finalSize);
            if (isValidRoi(roi))
            {
                trackerMgr->getTracker()->init(frame, roi);
                roiState.trackerReady = true;
            }
            else
            {
                std::cout << "Invalid ROI for tracking initialization" << std::endl;
            }
            roiState.initializeTracker = false;
        }

        if (roiState.trackerReady)
        {
            cv::Rect trackedRoi;
            if (trackerMgr->getTracker()->update(frame, trackedRoi))
            {
                if (isValidRoi(trackedRoi))
                {
                    cv::rectangle(frame, trackedRoi, cv::Scalar(0, 0, 255), 4);

                    cv::Mat roi = frame(trackedRoi);
                    if (!roi.empty())
                    {
                        cv::resize(roi, roi, cv::Size(320, 460));
                        cv::Rect destRoi(1, 1, 320, 460);
                        if (isValidRoi(destRoi))
                        {
                            roi.copyTo(frame(destRoi));
                        }
                    }
                }
            }
        }
    }
} VideoTracker;

int main()
{
    try
    {
        VIDEO_TRACKER tracker;
        tracker.processVideo();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}