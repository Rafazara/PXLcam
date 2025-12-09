#pragma once

namespace pxlcam::selftest {

/// Executa rotina completa de diagnóstico do hardware.
/// Testa: Serial, PSRAM, Display, Botão (GPIO12), Heap.
/// Após finalizar, entra em loop infinito.
/// Só é compilado quando -DPXLCAM_SELFTEST está definido.
void run();

}  // namespace pxlcam::selftest
