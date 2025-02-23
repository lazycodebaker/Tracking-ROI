#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <optional>

namespace VideoProcessing
{

    inline const std::string DEFAULT_VIDEO_PATH = "/Users/anshumantiwari/Documents/codes/personal/C++/tracking/track.mp4";
    constexpr int OUTPUT_WIDTH = 1024;
    constexpr int OUTPUT_HEIGHT = 800;

    class TrackerConfiguration
    {
    public:
        static constexpr double DEFAULT_FPS = 30.0;
        static inline const cv::Size DEFAULT_OUTPUT_SIZE = cv::Size(OUTPUT_WIDTH, OUTPUT_HEIGHT);
        static inline const int DEFAULT_CODEC = cv::VideoWriter::fourcc('X', '2', '6', '4');
    };

    // Moved Config outside VideoProcessor
    struct Config
    {
        std::string input_path = DEFAULT_VIDEO_PATH;
        std::string output_path = "tracked_output.mp4";
        cv::Size output_size = TrackerConfiguration::DEFAULT_OUTPUT_SIZE;
        int codec = TrackerConfiguration::DEFAULT_CODEC;
    };

    class TrackerManager
    {
    public:
        explicit TrackerManager()
        {
            tracker_ = cv::TrackerCSRT::create();
            if (!tracker_)
            {
                throw std::runtime_error("Failed to initialize CSRT tracker");
            }
        }

        [[nodiscard]] cv::Ptr<cv::Tracker> getTracker() const noexcept { return tracker_; }

    private:
        cv::Ptr<cv::Tracker> tracker_;
    };

    struct RegionOfInterest
    {
        cv::Point2i start_position{0, 0};
        cv::Point2i current_position{0, 0};
        cv::Size dimensions{0, 0};
        bool is_drawing{false};
        bool is_tracking{false};
        bool needs_initialization{false};
    };

    class VideoProcessor
    {
    public:
        explicit VideoProcessor(const Config &config = Config())
            : config_(config),
              tracker_manager_(std::make_unique<TrackerManager>()) {}

        void process()
        {
            initializeVideoCapture();
            initializeVideoOutput();
            setupWindowAndCallbacks();
            runProcessingLoop();
        }

    private:
        Config config_;
        cv::Mat current_frame_;
        RegionOfInterest roi_state_;
        std::unique_ptr<TrackerManager> tracker_manager_;
        std::optional<cv::VideoCapture> video_capture_;
        std::optional<cv::VideoWriter> video_writer_;
        static inline const std::string WINDOW_NAME = "VideoTracking";

        void initializeVideoCapture()
        {
            video_capture_.emplace(config_.input_path);
            if (!video_capture_->isOpened())
            {
                throw std::runtime_error("Failed to open video source: " + config_.input_path);
            }
        }

        void initializeVideoOutput()
        {
            video_writer_.emplace(
                config_.output_path,
                config_.codec,
                video_capture_->get(cv::CAP_PROP_FPS),
                config_.output_size,
                true);
            if (!video_writer_->isOpened())
            {
                throw std::runtime_error("Failed to initialize video writer");
            }
        }

        void setupWindowAndCallbacks()
        {
            cv::namedWindow(WINDOW_NAME, cv::WINDOW_AUTOSIZE);
            cv::setMouseCallback(WINDOW_NAME, mouseCallbackHandler, this);
        }

        [[nodiscard]] cv::Rect2d calculateNormalizedRoi(const cv::Point2i &start, const cv::Point2i &end) const
        {
            const auto x = std::max(0, std::min(start.x, end.x));
            const auto y = std::max(0, std::min(start.y, end.y));
            const auto width = std::min(std::abs(end.x - start.x), current_frame_.cols - x);
            const auto height = std::min(std::abs(end.y - start.y), current_frame_.rows - y);
            return {static_cast<double>(x), static_cast<double>(y),
                    static_cast<double>(width), static_cast<double>(height)};
        }

        [[nodiscard]] bool isValidRegion(const cv::Rect2d &roi) const noexcept
        {
            return roi.width > 0 && roi.height > 0 &&
                   roi.x >= 0 && roi.y >= 0 &&
                   (roi.x + roi.width) <= current_frame_.cols &&
                   (roi.y + roi.height) <= current_frame_.rows;
        }

        static void mouseCallbackHandler(int event, int x, int y, int flags, void *userdata)
        {
            auto *processor = static_cast<VideoProcessor *>(userdata);
            processor->handleMouseInteraction(event, x, y);
        }

        void handleMouseInteraction(int event, int x, int y)
        {
            switch (event)
            {
            case cv::EVENT_LBUTTONDOWN:
                roi_state_.start_position = {x, y};
                roi_state_.is_drawing = true;
                break;

            case cv::EVENT_MOUSEMOVE:
                roi_state_.current_position = {x, y};
                break;

            case cv::EVENT_LBUTTONUP:
            {
                const auto roi = calculateNormalizedRoi(roi_state_.start_position, {x, y});
                if (isValidRegion(roi))
                {
                    roi_state_.dimensions = {static_cast<int>(roi.width), static_cast<int>(roi.height)};
                    roi_state_.start_position = {static_cast<int>(roi.x), static_cast<int>(roi.y)};
                    roi_state_.needs_initialization = true;
                }
                roi_state_.is_drawing = false;
                break;
            }
            }
        }

        void processCurrentFrame()
        {
            if (roi_state_.is_drawing)
            {
                const auto roi = calculateNormalizedRoi(roi_state_.start_position,
                                                        roi_state_.current_position);
                if (isValidRegion(roi))
                {
                    cv::rectangle(current_frame_, roi, cv::Scalar(255, 255, 255), 4);
                }
            }

            if (roi_state_.needs_initialization)
            {
                const cv::Rect2d roi(roi_state_.start_position, roi_state_.dimensions);
                if (isValidRegion(roi))
                {
                    tracker_manager_->getTracker()->init(current_frame_, roi);
                    roi_state_.is_tracking = true;
                }
                roi_state_.needs_initialization = false;
            }

            if (roi_state_.is_tracking)
            {
                cv::Rect tracked_region;
                if (tracker_manager_->getTracker()->update(current_frame_, tracked_region))
                {
                    if (isValidRegion(tracked_region))
                    {
                        cv::rectangle(current_frame_, tracked_region, cv::Scalar(0, 0, 255), 4);
                        processTrackedRegion(tracked_region);
                    }
                }
            }
        }

        void processTrackedRegion(const cv::Rect2d &region)
        {
            cv::Rect int_region(static_cast<int>(region.x), static_cast<int>(region.y),
                                static_cast<int>(region.width), static_cast<int>(region.height));

            if (isValidRegion(region))
            {
                cv::Mat roi = current_frame_(int_region);
                if (!roi.empty())
                {
                    static const cv::Size preview_size{320, 460};
                    cv::resize(roi, roi, preview_size);
                    const cv::Rect preview_area{1, 1, preview_size.width, preview_size.height};
                    if (isValidRegion(preview_area))
                    {
                        roi.copyTo(current_frame_(preview_area));
                    }
                }
            }
        }

        void runProcessingLoop()
        {
            while (video_capture_->read(current_frame_))
            {
                cv::resize(current_frame_, current_frame_, config_.output_size);
                processCurrentFrame();
                video_writer_->write(current_frame_);
                cv::imshow(WINDOW_NAME, current_frame_);

                if (cv::waitKey(20) >= 0)
                {
                    break;
                }
            }
        }
    };
}

int main()
{
    try
    {
        VideoProcessing::VideoProcessor processor;
        processor.process();
        return EXIT_SUCCESS;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Fatal error: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}