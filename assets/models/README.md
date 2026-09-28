# Person segmentation model

`deeplabv3_person.tflite` is the TensorFlow Lite DeepLabV3 model published by
TensorFlow Hub at:

https://tfhub.dev/tensorflow/lite-model/deeplabv3/1/default/1

The model is distributed under the Apache License 2.0. Camora uses class 15
(person) from its 21-class output to build the live background-compositing mask.
