import 'dart:convert';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

enum GpuRuntimePhase { checking, unavailable, ready, downloading, error }

class GpuRuntimeManager extends ChangeNotifier {
  GpuRuntimePhase phase = GpuRuntimePhase.checking;
  String backend = 'CPU';
  String? driverVersion;
  String? error;
  double? progress;
  bool installed = false;
  bool restartRequired = false;
  Map<String, Object?>? _artifact;

  bool get canInstall =>
      phase != GpuRuntimePhase.downloading &&
      !installed &&
      driverVersion != null &&
      _artifact?['url'] is String;

  Future<void> initialize({Future<String> Function()? backendReader}) async {
    phase = GpuRuntimePhase.checking;
    notifyListeners();
    try {
      backend = backendReader == null ? 'CPU' : await backendReader();
      driverVersion = await _detectNvidiaDriver();
      _artifact = await _loadCompatibleArtifact(driverVersion);
      installed = await _isInstalled();
      phase = installed ? GpuRuntimePhase.ready : GpuRuntimePhase.unavailable;
      error = null;
    } catch (exception) {
      phase = GpuRuntimePhase.error;
      error = _friendlyError(exception);
    }
    notifyListeners();
  }

  Future<void> refreshBackend(Future<String> Function() reader) async {
    try {
      final current = await reader();
      if (backend == current) return;
      backend = current;
      notifyListeners();
    } catch (_) {
      // Status is informational and must never affect camera operation.
    }
  }

  Future<void> install() async {
    final artifact = _artifact;
    if (!canInstall || artifact == null) return;
    phase = GpuRuntimePhase.downloading;
    progress = 0;
    error = null;
    notifyListeners();

    final root = _runtimeRoot();
    final downloads = Directory('${root.path}/downloads');
    final staging = Directory('${root.path}/.installing');
    final archive = File('${downloads.path}/runtime-download');
    try {
      await downloads.create(recursive: true);
      if (await staging.exists()) await staging.delete(recursive: true);
      await staging.create(recursive: true);
      await _download(Uri.parse(artifact['url']! as String), archive);
      final actual = await _sha256(archive);
      if (actual != (artifact['sha256']! as String).toLowerCase()) {
        throw const FormatException('GPU runtime integrity check failed.');
      }
      await _extractVerifiedArchive(archive, staging);
      await _verifyPayload(staging);
      final version = artifact['version']! as String;
      final destination = Directory('${root.path}/$version');
      if (await destination.exists()) await destination.delete(recursive: true);
      await staging.rename(destination.path);
      final current = Link('${root.path}/current');
      if (await current.exists()) await current.delete();
      await current.create(version);
      installed = true;
      restartRequired = true;
      phase = GpuRuntimePhase.ready;
      progress = 1;
    } catch (exception) {
      phase = GpuRuntimePhase.error;
      error = _friendlyError(exception);
      progress = null;
      if (await staging.exists()) await staging.delete(recursive: true);
    } finally {
      if (await archive.exists()) await archive.delete();
      notifyListeners();
    }
  }

  Future<void> remove() async {
    try {
      final root = _runtimeRoot();
      if (await root.exists()) await root.delete(recursive: true);
      installed = false;
      restartRequired = backend == 'NVIDIA CUDA';
      phase = GpuRuntimePhase.unavailable;
      progress = null;
      error = null;
    } catch (exception) {
      phase = GpuRuntimePhase.error;
      error = _friendlyError(exception);
    }
    notifyListeners();
  }

  String get availabilityMessage {
    if (driverVersion == null) {
      return 'No compatible NVIDIA driver was detected. CPU remains active.';
    }
    if (_artifact == null || _artifact?['url'] is! String) {
      return 'GPU downloads are not configured for this release. CPU remains active.';
    }
    if (restartRequired) {
      return installed
          ? 'GPU runtime installed. Restart Camora to activate it.'
          : 'GPU runtime removed. Restart Camora to finish switching to CPU.';
    }
    if (installed && backend != 'NVIDIA CUDA') {
      return 'Installed. CUDA is verified when an AI effect starts.';
    }
    return installed
        ? 'GPU runtime is installed and verified at effect startup.'
        : 'A compatible NVIDIA GPU is available.';
  }

  Future<Map<String, Object?>?> _loadCompatibleArtifact(String? driver) async {
    final raw = await rootBundle.loadString(
      'assets/runtime/linux-gpu-runtime.json',
    );
    final manifest = jsonDecode(raw) as Map<String, Object?>;
    if (manifest['schemaVersion'] != 1) {
      throw const FormatException('Unsupported GPU runtime manifest.');
    }
    final value = manifest['artifact'];
    if (value == null) return null;
    final artifact = (value as Map).cast<String, Object?>();
    for (final key in ['version', 'sha256', 'minDriver']) {
      if (artifact[key] is! String || (artifact[key]! as String).isEmpty) {
        throw FormatException('GPU runtime manifest is missing $key.');
      }
    }
    final url = artifact['url'];
    if (url != null && (url is! String || url.isEmpty)) {
      throw const FormatException('GPU runtime manifest URL is invalid.');
    }
    if (!RegExp(r'^[a-fA-F0-9]{64}$').hasMatch(artifact['sha256']! as String)) {
      throw const FormatException('GPU runtime SHA-256 is invalid.');
    }
    if (driver == null ||
        !_versionAtLeast(driver, artifact['minDriver']! as String)) {
      return null;
    }
    return artifact;
  }

  Future<String?> _detectNvidiaDriver() async {
    try {
      final result = await Process.run('nvidia-smi', const [
        '--query-gpu=driver_version',
        '--format=csv,noheader',
      ]);
      if (result.exitCode != 0) return null;
      final versions = '${result.stdout}'
          .split('\n')
          .map((line) => line.trim())
          .where((line) => line.isNotEmpty)
          .toList();
      return versions.isEmpty ? null : versions.first;
    } on ProcessException {
      return null;
    }
  }

  Future<void> _download(Uri uri, File destination) async {
    if (uri.scheme != 'https') {
      throw const FormatException('GPU runtime download must use HTTPS.');
    }
    final client = HttpClient();
    try {
      final response = await (await client.getUrl(uri)).close();
      if (response.statusCode != HttpStatus.ok) {
        throw HttpException(
          'Download failed (${response.statusCode}).',
          uri: uri,
        );
      }
      final sink = destination.openWrite();
      var received = 0;
      await for (final chunk in response) {
        sink.add(chunk);
        received += chunk.length;
        progress = response.contentLength > 0
            ? received / response.contentLength
            : null;
        notifyListeners();
      }
      await sink.close();
    } finally {
      client.close(force: true);
    }
  }

  Future<String> _sha256(File archive) async {
    final result = await Process.run('sha256sum', [archive.path]);
    if (result.exitCode != 0) {
      throw StateError('Could not verify the download.');
    }
    return '${result.stdout}'.trim().split(RegExp(r'\s+')).first.toLowerCase();
  }

  Future<void> _extractVerifiedArchive(File archive, Directory staging) async {
    final listing = await Process.run('tar', ['-tzf', archive.path]);
    if (listing.exitCode != 0) {
      throw const FormatException('GPU runtime archive is invalid.');
    }
    for (final entry in '${listing.stdout}'.split('\n')) {
      if (entry.isEmpty) continue;
      if (entry.startsWith('/') || entry.split('/').contains('..')) {
        throw const FormatException(
          'GPU runtime archive contains an unsafe path.',
        );
      }
    }
    final result = await Process.run('tar', [
      '-xzf',
      archive.path,
      '-C',
      staging.path,
      '--no-same-owner',
      '--no-same-permissions',
    ]);
    if (result.exitCode != 0) {
      throw const FormatException('Could not unpack the GPU runtime.');
    }
  }

  Future<void> _verifyPayload(Directory directory) async {
    const required = [
      'libonnxruntime_providers_shared.so',
      'libonnxruntime_providers_cuda.so',
      'libcudart.so.13',
      'libcublas.so.13',
      'libcudnn.so.9',
    ];
    for (final name in required) {
      if (!await File('${directory.path}/lib/$name').exists()) {
        throw FormatException('GPU runtime is missing $name.');
      }
    }
  }

  Future<bool> _isInstalled() async {
    final directory = Directory('${_runtimeRoot().path}/current');
    if (!await directory.exists()) return false;
    try {
      await _verifyPayload(directory);
      return true;
    } catch (_) {
      return false;
    }
  }

  Directory _runtimeRoot() {
    final home = Platform.environment['HOME'];
    if (home == null || home.isEmpty) {
      throw StateError('The user data directory is unavailable.');
    }
    final xdgDataHome = Platform.environment['XDG_DATA_HOME'];
    return Directory(
      xdgDataHome == null || xdgDataHome.isEmpty
          ? '$home/.local/share/camora/runtime'
          : '$xdgDataHome/camora/runtime',
    );
  }

  bool _versionAtLeast(String actual, String required) {
    final left = actual.split('.').map(int.tryParse).toList();
    final right = required.split('.').map(int.tryParse).toList();
    for (var index = 0; index < left.length || index < right.length; index++) {
      final a = index < left.length ? left[index] ?? 0 : 0;
      final b = index < right.length ? right[index] ?? 0 : 0;
      if (a != b) return a > b;
    }
    return true;
  }

  String _friendlyError(Object exception) {
    final message = exception.toString().replaceFirst(
      RegExp(r'^\w+Exception: '),
      '',
    );
    return message.length <= 180 ? message : '${message.substring(0, 180)}…';
  }
}
