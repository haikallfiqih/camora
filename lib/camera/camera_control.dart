class CameraControlOption {
  final int value;
  final String label;

  const CameraControlOption({
    required this.value,
    required this.label,
  });

  factory CameraControlOption.fromJson(
    Map<String, dynamic> json,
  ) {
    return CameraControlOption(
      value: (json['value'] as num).toInt(),
      label: json['label'].toString(),
    );
  }
}

class CameraControl {
  final int id;
  final String name;
  final String type;

  final int min;
  final int max;
  final int step;
  final int defaultValue;

  int value;

  final bool inactive;

  final List<CameraControlOption> options;

  CameraControl({
    required this.id,
    required this.name,
    required this.type,
    required this.min,
    required this.max,
    required this.step,
    required this.defaultValue,
    required this.value,
    required this.inactive,
    required this.options,
  });

  factory CameraControl.fromJson(
    Map<String, dynamic> json,
  ) {
    return CameraControl(
      id: (json['id'] as num).toInt(),
      name: json['name'] as String,
      type: json['type'] as String,
      min: (json['min'] as num).toInt(),
      max: (json['max'] as num).toInt(),
      step: (json['step'] as num).toInt(),
      defaultValue:
          (json['default'] as num).toInt(),
      value: (json['value'] as num).toInt(),
      inactive: json['inactive'] as bool,
      options:
          (json['options'] as List<dynamic>? ?? [])
              .map(
                (item) =>
                    CameraControlOption.fromJson(
                  item as Map<String, dynamic>,
                ),
              )
              .toList(),
    );
  }
}
