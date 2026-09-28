# Subject matting model

`rvm_mobilenetv3_fp32.onnx` is the official MobileNetV3 Robust Video Matting
model published by the RVM project:

https://github.com/PeterL1n/RobustVideoMatting

RVM predicts a soft foreground alpha matte and carries recurrent state between
video frames. Camora runs it asynchronously through ONNX Runtime at an
aspect-correct working resolution.

The model and upstream project are distributed under the GNU General Public
License v3.0. The vendored ONNX Runtime headers are distributed under the MIT
License by Microsoft.
