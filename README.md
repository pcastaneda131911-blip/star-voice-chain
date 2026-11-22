# STAR Voice Chain (VST3)

Plugin VST3 de voz que analiza tu señal, selecciona una cadena inicial y permite usar los VST3 instalados en Windows.

## Funciones
- Escaneo de `C:\Program Files\Common Files\VST3`
- Reconocimiento básico por nombre para EQ, compresor, limitador, de-esser, ruido, aire y reverb
- Módulos internos como alternativa
- Apertura de interfaces gráficas de plugins externos
- Comandos de texto: "más brillante", "menos sibilancia", "pon afinación", "pon delay", "quita el delay", "quita la reverb"
- Guardado del estado del plugin dentro del proyecto

## Compilación local
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Instala `STAR.vst3` en:
`C:\Program Files\Common Files\VST3\`
