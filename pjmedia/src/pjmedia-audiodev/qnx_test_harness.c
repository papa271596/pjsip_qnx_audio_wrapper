
#include <pj/config.h>
#include <pjmedia-audiodev/qnx_dev.h>
#include <pjmedia_audiodev.h>
#include <sys/asoundlib.h>
#include <pjlib.h>
#include <pjmedia.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <unistd.h>

#define THIS_FILE "qnx_test_harness.c"

int main()
{
    pj_status_t status;
    pj_caching_pool cp;
    pjmedia_aud_dev_factory *factory;
    pjmedia_aud_param param;
    pjmedia_aud_stream *stream;
    unsigned dev_count;

    // Initialize PJLIB
    status = pj_init();
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "Failed to initialize PJLIB"));
        return 1;
    }

    // Initialize caching pool
    pj_caching_pool_init(&cp, NULL, 0);

    // Create QNX audio device factory
    factory = pjmedia_qnx_factory(&cp.factory);
    if (!factory) {
        PJ_LOG(1, (THIS_FILE, "Failed to create QNX audio device factory"));
        return 1;
    }

    // Initialize factory
    status = pjmedia_aud_dev_init(factory);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "Failed to initialize audio device factory"));
        return 1;
    }

    // Enumerate devices
    dev_count = pjmedia_aud_dev_count();
    PJ_LOG(3, (THIS_FILE, "Found %d audio devices", dev_count));

    for (unsigned i = 0; i < dev_count; ++i) {
        pjmedia_aud_dev_info info;
        pjmedia_aud_dev_get_info(i, &info);
        PJ_LOG(3, (THIS_FILE, "Device %d: %s [%s]", i, info.name, info.driver));
    }

    // Use default parameters for first device
    status = pjmedia_aud_dev_default_param(0, &param);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "Failed to get default parameters"));
        return 1;
    }

    // Create stream
    status = pjmedia_aud_dev_create_stream(&param, NULL, NULL, NULL, &stream);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "Failed to create audio stream"));
        return 1;
    }

    // Start stream
    status = pjmedia_aud_stream_start(stream);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "Failed to start audio stream"));
        pjmedia_aud_stream_destroy(stream);
        return 1;
    }

    PJ_LOG(3, (THIS_FILE, "Audio stream started successfully"));

    // Run for 5 seconds
    pj_thread_sleep(5000);

    // Stop and destroy stream
    pjmedia_aud_stream_stop(stream);
    pjmedia_aud_stream_destroy(stream);

    PJ_LOG(3, (THIS_FILE, "Audio stream stopped and destroyed"));

    // Shutdown factory
    pjmedia_aud_dev_shutdown();

    // Destroy caching pool
    pj_caching_pool_destroy(&cp);

    PJ_LOG(3, (THIS_FILE, "Test harness completed successfully"));
    return 0;
}
