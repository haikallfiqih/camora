class CameraFormat {
  const CameraFormat({
    required this.pixelFormat,
    required this.width,
    required this.height,
    required this.fps,
  });

  final String pixelFormat;
  final int width;
  final int height;
  final int fps;

  factory CameraFormat.fromJson(Map<String, dynamic> json) {
    return CameraFormat(
      pixelFormat: json['pixelFormat'] as String,
      width: json['width'] as int,
      height: json['height'] as int,
      fps: json['fps'] as int,
    );
  }

  String get resolution => '${width}x$height';

  String get label {
    final quality = switch (height) {
      >= 2160 => '4K',
      >= 1440 => '1440p',
      >= 1080 => '1080p',
      >= 720 => '720p',
      _ => '${height}p',
    };

    return '$quality · $fps FPS · $pixelFormat';
  }

  @override
  bool operator ==(Object other) {
    return other is CameraFormat &&
        other.pixelFormat == pixelFormat &&
        other.width == width &&
        other.height == height &&
        other.fps == fps;
  }

  @override
  int get hashCode => Object.hash(pixelFormat, width, height, fps);
}
