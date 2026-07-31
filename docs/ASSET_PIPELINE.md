# Asset Pipeline

- Основной формат runtime-моделей: `.glb`; самодостаточный `.gltf` с embedded
  buffers допустим для сторонних анимированных ассетов.
- Одна Blender unit равна одному метру.
- Имена файлов: `snake_case`.
- Origin ставится в точку установки объекта.
- Коллизия описывается отдельно от render mesh.
- Сокеты: `socket_muzzle`, `socket_base`, `socket_effect_*`.
- Базовые анимации: `idle`, `run`, `attack`, `hit`, `death`.
- Анимированные модели должны содержать skin, bones и совместимые animation
  clips; runtime raylib собирается с `SUPPORT_GPU_SKINNING=ON`.
- Односторонняя трава требует корректного winding и не должна полагаться на
  отключение culling для всего world pass.
- Лицензии и notice-файлы хранятся рядом с соответствующими ассетами.
- Runtime не читает файлы из `Downloads` или `Desktop`: используемые ресурсы
  копируются внутрь `assets/`.

Fallback-примитив допустим только при ошибке загрузки модели и должен быть
заметен в логах.
