// Stand-in for the certificate bundle symbol that the ESP32 core provides on the real chip.
extern "C" {
extern const unsigned char _binary_x509_crt_bundle_start[];
const unsigned char _binary_x509_crt_bundle_start[4] = {0, 0, 0, 0};
}
