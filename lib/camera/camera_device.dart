class CameraDevice {
  final String path;
  final String name;
  final String driver;
  final String bus;

  const CameraDevice({
    required this.path,
    required this.name,
    required this.driver,
    required this.bus,
  });

  factory CameraDevice.fromJson(Map<String, dynamic> json) {
    return CameraDevice(
      path: json['path'] as String,
      name: json['name'] as String,
      driver: json['driver'] as String,
      bus: json['bus'] as String,
    );
  }
}
