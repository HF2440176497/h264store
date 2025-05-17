

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <opencv2/opencv.hpp>

#include "pushwork.h"

namespace py = pybind11;
 

cv::Mat numpy_to_mat(py::array_t<uint8_t>& array) {
    py::buffer_info buf = array.request();
    if (buf.ndim != 2 && buf.ndim != 3) {
        throw std::runtime_error("Number of dimensions must be 2 or 3");
    }
    int channels = (buf.ndim == 3) ? buf.shape[2] : 1;
    cv::Mat mat(buf.shape[0], buf.shape[1], 
                (channels == 1) ? CV_8UC1 : CV_8UC3, 
                static_cast<uint8_t*>(buf.ptr));
    return mat.clone();
}


PYBIND11_MODULE(compressor, m) {
    py::class_<PushWork>(m, "PushWork")
        .def(py::init<int, int, int>(),
             py::arg("queue_size"),
             py::arg("width"),
             py::arg("height"))
        .def("init", &PushWork::init)
        .def("stop", &PushWork::stop)
        .def("put_data", [](PushWork& self, py::array_t<uint8_t> arr) {
            cv::Mat mat = numpy_to_mat(arr);
            return self.put_data(mat);
        });
}