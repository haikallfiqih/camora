#include "my_application.h"

#include <flutter_linux/flutter_linux.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#include "flutter/generated_plugin_registrant.h"
#include "camora_texture.h"
#include "../../native/capture_engine.h"
#include "../../native/background_media_source.h"
#include "../../native/virtual_camera_output.h"
#include "../../native/camora_v4l2.h"

#include <algorithm>
#include <cmath>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

struct _MyApplication {
  GtkApplication parent_instance;
  char** dart_entrypoint_arguments;
};


G_DEFINE_TYPE(MyApplication, my_application, GTK_TYPE_APPLICATION)

static CaptureEngine g_camora_capture;
static BackgroundMediaSource g_background_media(g_camora_capture);
static VirtualCameraOutput g_virtual_camera(g_camora_capture);

static FlTextureRegistrar* g_texture_registrar = nullptr;
static CamoraTexture* g_camora_texture = nullptr;

static std::atomic<bool> g_texture_notifier_running{false};
static std::thread g_texture_notifier;
static bool g_background_enabled = false;
static std::string g_background_path;
static std::string g_loaded_background_path;
static int g_loaded_background_width = 0;
static int g_loaded_background_height = 0;
static bool g_background_blur_enabled = false;
static int g_background_blur_strength = 70;
static bool g_background_removal_enabled = false;
static bool g_auto_framing_enabled = false;
static int g_auto_framing_sensitivity = 50;

static bool ensure_segmentation_model() {
  if (g_camora_capture.backgroundReplacementAvailable()) return true;

  g_autoptr(GError) error = nullptr;
  g_autofree gchar* executable =
      g_file_read_link("/proc/self/exe", &error);
  if (!executable) {
    g_warning("Could not resolve Camora executable: %s",
              error ? error->message : "unknown error");
    return false;
  }
  g_autofree gchar* directory = g_path_get_dirname(executable);
  g_autofree gchar* model_path = g_build_filename(
      directory,
      "data",
      "flutter_assets",
      "assets",
      "models",
      "rvm_mobilenetv3_fp32.onnx",
      nullptr);
  return g_camora_capture.configureSegmentationModel(model_path);
}

static bool update_background_replacement() {
  if (!g_background_enabled || g_background_path.empty()) {
    g_background_media.stop();
    g_loaded_background_path.clear();
    g_loaded_background_width = 0;
    g_loaded_background_height = 0;
    g_camora_capture.setBackgroundReplacement(false, {}, 0, 0);
    return true;
  }
  if (!ensure_segmentation_model()) return false;

  const int target_width = g_camora_capture.width();
  const int target_height = g_camora_capture.height();
  if (target_width <= 0 || target_height <= 0) return true;

  if (g_loaded_background_path == g_background_path &&
      g_loaded_background_width == target_width &&
      g_loaded_background_height == target_height) {
    return true;
  }

  std::string error;
  if (!g_background_media.start(
          g_background_path, target_width, target_height, error)) {
    g_warning("Could not load background media: %s", error.c_str());
    g_camora_capture.setBackgroundReplacement(false, {}, 0, 0);
    return false;
  }

  g_loaded_background_path = g_background_path;
  g_loaded_background_width = target_width;
  g_loaded_background_height = target_height;
  return true;
}

static void stop_camora_video() {
  g_virtual_camera.stop();
  g_background_media.stop();
  g_loaded_background_path.clear();
  g_loaded_background_width = 0;
  g_loaded_background_height = 0;
  g_texture_notifier_running = false;

  if (g_texture_notifier.joinable()) {
    g_texture_notifier.join();
  }

  if (g_camora_texture) {
    camora_texture_shutdown(g_camora_texture);
  }

  g_camora_capture.stop();

  if (g_texture_registrar && g_camora_texture) {
    fl_texture_registrar_unregister_texture(
        g_texture_registrar,
        FL_TEXTURE(g_camora_texture));
  }

  if (g_camora_texture) {
    g_object_unref(g_camora_texture);
    g_camora_texture = nullptr;
  }
}

static void video_method_call_cb(
    FlMethodChannel* channel,
    FlMethodCall* method_call,
    gpointer user_data) {

  const gchar* method =
      fl_method_call_get_name(method_call);

  if (strcmp(method, "start") == 0) {
    stop_camora_video();

    FlValue* args =
        fl_method_call_get_args(method_call);

    const gchar* device = "/dev/video2";
    int width = 1920;
    int height = 1080;
    int fps = 30;

    if (args &&
        fl_value_get_type(args) == FL_VALUE_TYPE_MAP) {

      FlValue* device_value =
          fl_value_lookup_string(args, "device");

      FlValue* width_value =
          fl_value_lookup_string(args, "width");

      FlValue* height_value =
          fl_value_lookup_string(args, "height");

      FlValue* fps_value =
          fl_value_lookup_string(args, "fps");

      if (device_value) {
        device =
            fl_value_get_string(device_value);
      }

      if (width_value) {
        width = static_cast<int>(
            fl_value_get_int(width_value));
      }

      if (height_value) {
        height = static_cast<int>(
            fl_value_get_int(height_value));
      }

      if (fps_value) {
        fps = static_cast<int>(
            fl_value_get_int(fps_value));
      }
    }

    if (!g_camora_capture.start(
            device,
            width,
            height,
            fps)) {

      g_autoptr(FlMethodResponse) response =
          FL_METHOD_RESPONSE(
              fl_method_error_response_new(
                  "capture_failed",
                  "Failed to start camera",
                  nullptr));

      fl_method_call_respond(
          method_call,
          response,
          nullptr);

      return;
    }

    if (g_background_enabled && !update_background_replacement()) {
      g_warning("Background replacement could not be initialized");
    }

    g_camora_texture =
        camora_texture_new(
            &g_camora_capture);

    if (!fl_texture_registrar_register_texture(
            g_texture_registrar,
            FL_TEXTURE(g_camora_texture))) {

      g_camora_capture.stop();

      g_object_unref(
          g_camora_texture);

      g_camora_texture = nullptr;

      g_autoptr(FlMethodResponse) response =
          FL_METHOD_RESPONSE(
              fl_method_error_response_new(
                  "texture_failed",
                  "Failed to register texture",
                  nullptr));

      fl_method_call_respond(
          method_call,
          response,
          nullptr);

      return;
    }

    const int64_t texture_id =
        reinterpret_cast<int64_t>(
            g_camora_texture);

    g_texture_notifier_running = true;

    g_texture_notifier =
        std::thread([]() {
          while (g_texture_notifier_running) {

            g_main_context_invoke(
                nullptr,
                [](gpointer data) -> gboolean {

                  if (
                      g_texture_registrar &&
                      g_camora_texture &&
                      g_texture_notifier_running
                  ) {
                    fl_texture_registrar_mark_texture_frame_available(
                        g_texture_registrar,
                        FL_TEXTURE(g_camora_texture));
                  }

                  return G_SOURCE_REMOVE;
                },
                nullptr);

            std::this_thread::sleep_for(
                std::chrono::milliseconds(33));
          }
        });

    g_autoptr(FlValue) result =
        fl_value_new_int(texture_id);

    g_autoptr(FlMethodResponse) response =
        FL_METHOD_RESPONSE(
            fl_method_success_response_new(
                result));

    fl_method_call_respond(
        method_call,
        response,
        nullptr);

    return;
  }

  if (strcmp(method, "setEffects") == 0) {
    FlValue* args = fl_method_call_get_args(method_call);
    bool low_light_enabled = false;
    int low_light_strength = 50;
    bool camera_mirrored = false;
    bool background_mirrored = false;

    if (args && fl_value_get_type(args) == FL_VALUE_TYPE_MAP) {
      FlValue* enabled_value =
          fl_value_lookup_string(args, "lowLightEnabled");
      FlValue* strength_value =
          fl_value_lookup_string(args, "lowLightStrength");
      FlValue* background_enabled_value =
          fl_value_lookup_string(args, "backgroundImageEnabled");
      FlValue* camera_mirrored_value =
          fl_value_lookup_string(args, "cameraMirrored");
      FlValue* background_mirrored_value =
          fl_value_lookup_string(args, "backgroundMirrored");
      FlValue* background_path_value =
          fl_value_lookup_string(args, "backgroundImagePath");
      FlValue* background_blur_enabled_value =
          fl_value_lookup_string(args, "backgroundBlurEnabled");
      FlValue* background_blur_strength_value =
          fl_value_lookup_string(args, "backgroundBlurStrength");
      FlValue* background_removal_enabled_value =
          fl_value_lookup_string(args, "backgroundRemovalEnabled");
      FlValue* auto_framing_enabled_value =
          fl_value_lookup_string(args, "autoFramingEnabled");
      FlValue* auto_framing_sensitivity_value =
          fl_value_lookup_string(args, "autoFramingSensitivity");

      if (enabled_value &&
          fl_value_get_type(enabled_value) == FL_VALUE_TYPE_BOOL) {
        low_light_enabled = fl_value_get_bool(enabled_value);
      }
      if (strength_value &&
          fl_value_get_type(strength_value) == FL_VALUE_TYPE_INT) {
        low_light_strength = static_cast<int>(
            fl_value_get_int(strength_value));
      }
      if (background_enabled_value &&
          fl_value_get_type(background_enabled_value) == FL_VALUE_TYPE_BOOL) {
        g_background_enabled =
            fl_value_get_bool(background_enabled_value);
      }
      if (camera_mirrored_value &&
          fl_value_get_type(camera_mirrored_value) == FL_VALUE_TYPE_BOOL) {
        camera_mirrored = fl_value_get_bool(camera_mirrored_value);
      }
      if (background_mirrored_value &&
          fl_value_get_type(background_mirrored_value) == FL_VALUE_TYPE_BOOL) {
        background_mirrored = fl_value_get_bool(background_mirrored_value);
      }
      if (background_path_value &&
          fl_value_get_type(background_path_value) == FL_VALUE_TYPE_STRING) {
        g_background_path = fl_value_get_string(background_path_value);
      } else {
        g_background_path.clear();
      }

      if (background_blur_enabled_value &&
          fl_value_get_type(background_blur_enabled_value) ==
              FL_VALUE_TYPE_BOOL) {
        g_background_blur_enabled =
            fl_value_get_bool(background_blur_enabled_value);
      }

      if (background_blur_strength_value &&
          fl_value_get_type(background_blur_strength_value) ==
              FL_VALUE_TYPE_INT) {
        g_background_blur_strength = static_cast<int>(
            fl_value_get_int(background_blur_strength_value));
      }

      if (background_removal_enabled_value &&
          fl_value_get_type(background_removal_enabled_value) ==
              FL_VALUE_TYPE_BOOL) {
        g_background_removal_enabled =
            fl_value_get_bool(background_removal_enabled_value);
      }

      if (auto_framing_enabled_value &&
          fl_value_get_type(auto_framing_enabled_value) ==
              FL_VALUE_TYPE_BOOL) {
        g_auto_framing_enabled =
            fl_value_get_bool(auto_framing_enabled_value);
      }

      if (auto_framing_sensitivity_value &&
          fl_value_get_type(auto_framing_sensitivity_value) ==
              FL_VALUE_TYPE_INT) {
        g_auto_framing_sensitivity = static_cast<int>(
            fl_value_get_int(auto_framing_sensitivity_value));
      }
    }

    g_camora_capture.setLowLightEnhancement(
        low_light_enabled, low_light_strength);
    g_camora_capture.setCameraMirrored(camera_mirrored);
    g_camora_capture.setBackgroundMirrored(background_mirrored);

    bool effects_available = true;

    if (g_background_blur_enabled ||
        g_background_removal_enabled ||
        g_background_enabled ||
        g_auto_framing_enabled) {
      effects_available = ensure_segmentation_model();
    }

    // Segmentation background effects are mutually exclusive.
    if (g_background_removal_enabled) {
      g_background_blur_enabled = false;
      g_background_enabled = false;
    } else if (g_background_blur_enabled) {
      g_background_enabled = false;
    }

    g_camora_capture.setBackgroundRemoval(
        g_background_removal_enabled && effects_available);

    g_camora_capture.setBackgroundBlur(
        g_background_blur_enabled && effects_available,
        g_background_blur_strength);

    g_camora_capture.setAutoFraming(
        g_auto_framing_enabled && effects_available,
        g_auto_framing_sensitivity);

    if (!update_background_replacement()) {
      effects_available = false;
    }

    g_autoptr(FlValue) result = fl_value_new_bool(effects_available);
    g_autoptr(FlMethodResponse) response = FL_METHOD_RESPONSE(
        fl_method_success_response_new(result));
    fl_method_call_respond(method_call, response, nullptr);
    return;
  }
  if (strcmp(method, "startVirtualCamera") == 0) {
    std::string error;
    const bool started = g_virtual_camera.start(
        g_camora_capture.width(),
        g_camora_capture.height(),
        g_camora_capture.fps(),
        error);

    if (!started) {
      g_autoptr(FlMethodResponse) response = FL_METHOD_RESPONSE(
          fl_method_error_response_new(
              "virtual_camera_failed", error.c_str(), nullptr));
      fl_method_call_respond(method_call, response, nullptr);
      return;
    }

    g_autoptr(FlValue) result = fl_value_new_bool(true);
    g_autoptr(FlMethodResponse) response = FL_METHOD_RESPONSE(
        fl_method_success_response_new(result));
    fl_method_call_respond(method_call, response, nullptr);
    return;
  }

  if (strcmp(method, "stopVirtualCamera") == 0) {
    g_virtual_camera.stop();
    g_autoptr(FlMethodResponse) response = FL_METHOD_RESPONSE(
        fl_method_success_response_new(nullptr));
    fl_method_call_respond(method_call, response, nullptr);
    return;
  }

  if (strcmp(method, "virtualCameraStatus") == 0) {
    g_autoptr(FlValue) result = fl_value_new_map();
    fl_value_set_string_take(
        result, "running", fl_value_new_bool(g_virtual_camera.running()));
    fl_value_set_string_take(
        result, "message",
        fl_value_new_string(g_virtual_camera.statusMessage().c_str()));
    fl_value_set_string_take(
        result, "name", fl_value_new_string("Camora Virtual Camera"));
    g_autoptr(FlMethodResponse) response = FL_METHOD_RESPONSE(
        fl_method_success_response_new(result));
    fl_method_call_respond(method_call, response, nullptr);
    return;
  }

  if (strcmp(method, "stop") == 0) {
    stop_camora_video();

    g_autoptr(FlMethodResponse) response =
        FL_METHOD_RESPONSE(
            fl_method_success_response_new(
                nullptr));

    fl_method_call_respond(
        method_call,
        response,
        nullptr);

    return;
  }

  g_autoptr(FlMethodResponse) response =
      FL_METHOD_RESPONSE(
          fl_method_not_implemented_response_new());

  fl_method_call_respond(
      method_call,
      response,
      nullptr);
}



static void camera_method_call_cb(
    FlMethodChannel* channel,
    FlMethodCall* method_call,
    gpointer user_data) {

  const gchar* method =
      fl_method_call_get_name(method_call);

  FlValue* args =
      fl_method_call_get_args(method_call);

  if (strcmp(method, "listCameras") == 0) {
    const char* json =
        camora_list_cameras();

    g_autoptr(FlValue) result =
        fl_value_new_string(json);

    g_autoptr(FlMethodResponse) response =
        FL_METHOD_RESPONSE(
            fl_method_success_response_new(
                result));

    fl_method_call_respond(
        method_call,
        response,
        nullptr);

    return;
  }

  if (strcmp(method, "listControls") == 0 ||
      strcmp(method, "listFormats") == 0) {

    if (!args ||
        fl_value_get_type(args) !=
            FL_VALUE_TYPE_MAP) {

      g_autoptr(FlMethodResponse) response =
          FL_METHOD_RESPONSE(
              fl_method_error_response_new(
                  "invalid_arguments",
                  "Expected argument map",
                  nullptr));

      fl_method_call_respond(
          method_call,
          response,
          nullptr);

      return;
    }

    FlValue* device_value =
        fl_value_lookup_string(
            args,
            "device");

    if (!device_value ||
        fl_value_get_type(device_value) !=
            FL_VALUE_TYPE_STRING) {

      g_autoptr(FlMethodResponse) response =
          FL_METHOD_RESPONSE(
              fl_method_error_response_new(
                  "invalid_device",
                  "Missing camera device",
                  nullptr));

      fl_method_call_respond(
          method_call,
          response,
          nullptr);

      return;
    }

    const char* device =
        fl_value_get_string(
            device_value);

    const char* json =
        strcmp(method, "listControls") == 0
            ? camora_list_controls(device)
            : camora_list_formats(device);

    g_autoptr(FlValue) result =
        fl_value_new_string(json);

    g_autoptr(FlMethodResponse) response =
        FL_METHOD_RESPONSE(
            fl_method_success_response_new(
                result));

    fl_method_call_respond(
        method_call,
        response,
        nullptr);

    return;
  }

  if (strcmp(method, "setControl") == 0) {
    if (!args ||
        fl_value_get_type(args) !=
            FL_VALUE_TYPE_MAP) {

      g_autoptr(FlMethodResponse) response =
          FL_METHOD_RESPONSE(
              fl_method_error_response_new(
                  "invalid_arguments",
                  "Expected argument map",
                  nullptr));

      fl_method_call_respond(
          method_call,
          response,
          nullptr);

      return;
    }

    FlValue* device_value =
        fl_value_lookup_string(
            args,
            "device");

    FlValue* id_value =
        fl_value_lookup_string(
            args,
            "id");

    FlValue* value_value =
        fl_value_lookup_string(
            args,
            "value");

    if (!device_value ||
        !id_value ||
        !value_value) {

      g_autoptr(FlMethodResponse) response =
          FL_METHOD_RESPONSE(
              fl_method_error_response_new(
                  "invalid_arguments",
                  "device, id and value are required",
                  nullptr));

      fl_method_call_respond(
          method_call,
          response,
          nullptr);

      return;
    }

    const char* device =
        fl_value_get_string(
            device_value);

    const uint32_t id =
        static_cast<uint32_t>(
            fl_value_get_int(id_value));

    const int32_t value =
        static_cast<int32_t>(
            fl_value_get_int(value_value));

    const int native_result =
        camora_set_control(
            device,
            id,
            value);

    g_autoptr(FlValue) result =
        fl_value_new_bool(
            native_result == 0);

    g_autoptr(FlMethodResponse) response =
        FL_METHOD_RESPONSE(
            fl_method_success_response_new(
                result));

    fl_method_call_respond(
        method_call,
        response,
        nullptr);

    return;
  }

  g_autoptr(FlMethodResponse) response =
      FL_METHOD_RESPONSE(
          fl_method_not_implemented_response_new());

  fl_method_call_respond(
      method_call,
      response,
      nullptr);
}


// Called when first Flutter frame received.
static void first_frame_cb(MyApplication* self, FlView* view) {
  gtk_widget_show(gtk_widget_get_toplevel(GTK_WIDGET(view)));
}

// Implements GApplication::activate.
static void my_application_activate(GApplication* application) {
  MyApplication* self = MY_APPLICATION(application);
  GtkWindow* window =
      GTK_WINDOW(gtk_application_window_new(GTK_APPLICATION(application)));

  // Use a header bar when running in GNOME as this is the common style used
  // by applications and is the setup most users will be using (e.g. Ubuntu
  // desktop).
  // If running on X and not using GNOME then just use a traditional title bar
  // in case the window manager does more exotic layout, e.g. tiling.
  // If running on Wayland assume the header bar will work (may need changing
  // if future cases occur).
  gboolean use_header_bar = TRUE;
#ifdef GDK_WINDOWING_X11
  GdkScreen* screen = gtk_window_get_screen(window);
  if (GDK_IS_X11_SCREEN(screen)) {
    const gchar* wm_name = gdk_x11_screen_get_window_manager_name(screen);
    if (g_strcmp0(wm_name, "GNOME Shell") != 0) {
      use_header_bar = FALSE;
    }
  }
#endif
  if (use_header_bar) {
    GtkHeaderBar* header_bar = GTK_HEADER_BAR(gtk_header_bar_new());
    gtk_widget_show(GTK_WIDGET(header_bar));
    gtk_header_bar_set_title(header_bar, "camora");
    gtk_header_bar_set_show_close_button(header_bar, TRUE);
    gtk_window_set_titlebar(window, GTK_WIDGET(header_bar));
  } else {
    gtk_window_set_title(window, "camora");
  }

  gtk_window_set_default_size(window, 1280, 720);

  g_autoptr(FlDartProject) project = fl_dart_project_new();
  fl_dart_project_set_dart_entrypoint_arguments(
      project, self->dart_entrypoint_arguments);

  FlView* view = fl_view_new(project);
  GdkRGBA background_color;
  // Background defaults to black, override it here if necessary, e.g. #00000000
  // for transparent.
  gdk_rgba_parse(&background_color, "#000000");
  fl_view_set_background_color(view, &background_color);
  gtk_widget_show(GTK_WIDGET(view));
  gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(view));

  // Show the window when Flutter renders.
  // Requires the view to be realized so we can start rendering.
  g_signal_connect_swapped(view, "first-frame", G_CALLBACK(first_frame_cb),
                           self);
  gtk_widget_realize(GTK_WIDGET(view));

  fl_register_plugins(FL_PLUGIN_REGISTRY(view));

  FlEngine* engine = fl_view_get_engine(view);

  g_texture_registrar =
      fl_engine_get_texture_registrar(engine);

  g_autoptr(FlStandardMethodCodec) codec =
      fl_standard_method_codec_new();

  FlMethodChannel* video_channel =
      fl_method_channel_new(
          fl_engine_get_binary_messenger(engine),
          "dev.tapticlabs.camora/video",
          FL_METHOD_CODEC(codec));

  fl_method_channel_set_method_call_handler(
      video_channel,
      video_method_call_cb,
      nullptr,
      nullptr);

  FlMethodChannel* camera_channel =
      fl_method_channel_new(
          fl_engine_get_binary_messenger(engine),
          "dev.tapticlabs.camora/camera",
          FL_METHOD_CODEC(codec));

  fl_method_channel_set_method_call_handler(
      camera_channel,
      camera_method_call_cb,
      nullptr,
      nullptr);

  gtk_widget_grab_focus(GTK_WIDGET(view));
}

// Implements GApplication::local_command_line.
static gboolean my_application_local_command_line(GApplication* application,
                                                  gchar*** arguments,
                                                  int* exit_status) {
  MyApplication* self = MY_APPLICATION(application);
  // Strip out the first argument as it is the binary name.
  self->dart_entrypoint_arguments = g_strdupv(*arguments + 1);

  g_autoptr(GError) error = nullptr;
  if (!g_application_register(application, nullptr, &error)) {
    g_warning("Failed to register: %s", error->message);
    *exit_status = 1;
    return TRUE;
  }

  g_application_activate(application);
  *exit_status = 0;

  return TRUE;
}

// Implements GApplication::startup.
static void my_application_startup(GApplication* application) {
  // MyApplication* self = MY_APPLICATION(object);

  // Perform any actions required at application startup.

  G_APPLICATION_CLASS(my_application_parent_class)->startup(application);
}

// Implements GApplication::shutdown.
static void my_application_shutdown(GApplication* application) {
  stop_camora_video();

  G_APPLICATION_CLASS(my_application_parent_class)->shutdown(application);
}

// Implements GObject::dispose.
static void my_application_dispose(GObject* object) {
  MyApplication* self = MY_APPLICATION(object);
  g_clear_pointer(&self->dart_entrypoint_arguments, g_strfreev);
  G_OBJECT_CLASS(my_application_parent_class)->dispose(object);
}

static void my_application_class_init(MyApplicationClass* klass) {
  G_APPLICATION_CLASS(klass)->activate = my_application_activate;
  G_APPLICATION_CLASS(klass)->local_command_line =
      my_application_local_command_line;
  G_APPLICATION_CLASS(klass)->startup = my_application_startup;
  G_APPLICATION_CLASS(klass)->shutdown = my_application_shutdown;
  G_OBJECT_CLASS(klass)->dispose = my_application_dispose;
}

static void my_application_init(MyApplication* self) {}

MyApplication* my_application_new() {
  // Set the program name to the application ID, which helps various systems
  // like GTK and desktop environments map this running application to its
  // corresponding .desktop file. This ensures better integration by allowing
  // the application to be recognized beyond its binary name.
  g_set_prgname(APPLICATION_ID);

  return MY_APPLICATION(g_object_new(my_application_get_type(),
                                     "application-id", APPLICATION_ID, "flags",
                                     G_APPLICATION_NON_UNIQUE, nullptr));
}
