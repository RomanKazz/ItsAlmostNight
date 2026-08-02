(() => {
  "use strict";

  const GRID_X = 200;
  const GRID_Y = 190;
  const GRID_ORIGIN = 40;
  const NODE_SIZE = 156;
  const STORAGE_KEY = "its-almost-night-skill-tree-v3";

  const branches = {
    root: { label: "Основа", color: "#d8bd72", icon: "✦" },
    axe: { label: "Топор", color: "#d77846", icon: "◆" },
    pickaxe: { label: "Кирка", color: "#62a8d8", icon: "◇" },
    club: { label: "Дубинка", color: "#b67a55", icon: "●" },
    hybrid: { label: "Сочетания", color: "#9d79d1", icon: "⌁" },
    mastery: { label: "Мастерство", color: "#70b98a", icon: "✧" }
  };

  const p = (col, row) => ({ x: GRID_ORIGIN + col * GRID_X, y: GRID_ORIGIN + row * GRID_Y });
  const node = (id, title, description, branch, cost, col, row) => ({ id, title, description, branch, cost, ...p(col, row) });

  const defaultNodes = [
    node("spark", "Искра ремесла", "Первый осознанный шаг: руки становятся инструментом выживания.", "root", 0, 5, 4),
    node("axe", "Топор", "Открывает рубку дерева и быстрые размашистые удары.", "axe", 1, 3, 3),
    node("pickaxe", "Кирка", "Открывает добычу камня, руды и кристаллов.", "pickaxe", 1, 5, 2),
    node("club", "Дубинка", "Первое настоящее оружие для ближнего боя.", "club", 1, 7, 3),
    node("clean-cut", "Чистый срез", "Точный удар по метке сразу срубает дополнительный кусок дерева.", "axe", 2, 2, 2),
    node("falling-lesson", "Падающий урок", "Срубленное дерево валится в выбранную сторону и ранит врагов.", "axe", 3, 1, 4),
    node("resin-hunter", "Охотник за смолой", "В стволах иногда появляется смола для огня и взрывчатки.", "axe", 3, 1, 1),
    node("living-wood", "Живое дерево", "Пни медленно отращивают дерево заново, если рядом есть свободное место.", "axe", 5, 0, 0),
    node("weak-spots", "Слабые места", "На камне проявляются точки; попадание раскалывает соседний пласт.", "pickaxe", 2, 4, 1),
    node("deep-vein", "Глубокая жила", "Последний удар может открыть редкую жилу внутри породы.", "pickaxe", 3, 5, 0),
    node("echo-stone", "Эхо камня", "Удар отмечает ближайшее скрытое месторождение коротким импульсом.", "pickaxe", 3, 6, 1),
    node("fault-line", "Линия разлома", "Серия точных ударов запускает трещину через несколько залежей.", "pickaxe", 5, 5, 1),
    node("wide-swing", "Широкий замах", "Удар цепляет нескольких врагов, но требует полного замаха.", "club", 2, 8, 2),
    node("brace", "Упор", "Удержание атаки превращает дубинку в краткую защитную стойку.", "club", 2, 9, 4),
    node("return-blow", "Ответный удар", "После успешного блока следующая атака отбрасывает сильнее.", "club", 4, 9, 1),
    node("ground-pound", "Удар оземь", "Атака в прыжке создаёт круговую волну и оглушает мелких врагов.", "club", 6, 10, 0),
    node("right-tool", "Нужный инструмент", "Топор и кирка автоматически сменяются при наведении на ресурс.", "hybrid", 4, 3, 1),
    node("breaker", "Ломатель", "Удар по оглушённому врагу раскалывает броню и добываемые наросты.", "hybrid", 4, 7, 1),
    node("hewn-stakes", "Тёсаные колья", "Открывает простые деревянные оборонительные сооружения.", "hybrid", 3, 5, 3),
    node("work-rhythm", "Рабочий ритм", "Удержание кнопки продолжает добычу, а точные удары ускоряют следующий.", "hybrid", 5, 2, 0),
    node("no-hesitation", "Без заминки", "Смена инструмента сохраняет фазу замаха и не прерывает движение.", "hybrid", 5, 3, 4),
    node("resource-magnet", "Подбор на ходу", "Свежедобытые ресурсы летят к игроку и не тормозят работу.", "mastery", 4, 2, 5),
    node("rough-foundation", "Грубый фундамент", "Открывает первый строительный блок и привязку к сетке.", "mastery", 2, 4, 5),
    node("palisade", "Частокол", "Открывает дешёвую стену, которая ранит атакующих.", "mastery", 3, 5, 5),
    node("spring-trap", "Пружинная ловушка", "Подбрасывает группу врагов и сбивает их атаку.", "mastery", 4, 6, 5),
    node("repair-beat", "Ремонтный такт", "Удар инструментом ремонтирует постройку; точная серия чинит соседние.", "mastery", 4, 3, 6),
    node("area-blueprint", "Площадной чертёж", "Позволяет протягивать платформы прямоугольной областью.", "mastery", 6, 4, 7),
    node("salvage-chain", "Цепной демонтаж", "Разбор опоры подсвечивает всё, что рухнет вместе с ней.", "mastery", 6, 5, 8),
    node("bomb-recipe", "Рецепт бомбы", "Создаёт бросаемую бомбу из камня, смолы и кристальной пыли.", "hybrid", 5, 7, 5),
    node("powder-trail", "Пороховой след", "Бомбы можно соединять дорожкой и подрывать в нужный момент.", "hybrid", 6, 8, 6),
    node("chain-reaction", "Цепная реакция", "Взрыв заряжает ловушки и передаётся по защитным сооружениям.", "hybrid", 8, 7, 7),
    node("sling", "Праща", "Открывает дешёвую дальнюю атаку найденными камнями.", "club", 3, 8, 4),
    node("crystal-shot", "Кристальный выстрел", "Заряд раскалывается в цели и помечает её для турелей.", "hybrid", 7, 10, 5),
    node("core-instinct", "Инстинкт Ядра", "Показывает направление опасности и позволяет один раз перехватить удар.", "mastery", 5, 6, 6),
    node("last-light", "Последний свет", "При критическом здоровье Ядро на миг оживляет все ловушки и турели.", "mastery", 10, 6, 8)
  ];

  const defaultEdges = [
    ["spark","axe"],["spark","pickaxe"],["spark","club"],
    ["axe","clean-cut"],["axe","falling-lesson"],["axe","right-tool"],["axe","hewn-stakes"],
    ["clean-cut","resin-hunter"],["resin-hunter","living-wood"],["resin-hunter","bomb-recipe"],
    ["pickaxe","weak-spots"],["pickaxe","deep-vein"],["pickaxe","echo-stone"],["pickaxe","right-tool"],["pickaxe","breaker"],
    ["weak-spots","fault-line"],["echo-stone","fault-line"],
    ["club","wide-swing"],["club","brace"],["club","breaker"],["club","hewn-stakes"],["club","sling"],
    ["wide-swing","return-blow"],["brace","return-blow"],["return-blow","ground-pound"],
    ["right-tool","work-rhythm"],["right-tool","no-hesitation"],["right-tool","rough-foundation"],
    ["work-rhythm","no-hesitation"],["work-rhythm","resource-magnet"],
    ["hewn-stakes","palisade"],["hewn-stakes","spring-trap"],["rough-foundation","repair-beat"],["rough-foundation","core-instinct"],
    ["palisade","area-blueprint"],["repair-beat","area-blueprint"],["area-blueprint","salvage-chain"],
    ["deep-vein","bomb-recipe"],["bomb-recipe","powder-trail"],["spring-trap","chain-reaction"],["powder-trail","chain-reaction"],
    ["sling","crystal-shot"],["deep-vein","crystal-shot"],["core-instinct","last-light"],["salvage-chain","last-light"]
  ].map(([from, to]) => ({ from, to }));

  const defaultNotes = [
    "Первый нод должен ощущаться как начало ремесла, а не меню выбора класса.",
    "Объединение двух веток должно менять поведение игры, а не только числа.",
    "Сильные автоматизации открываются после ручного освоения действия."
  ];

  let state = loadState();
  let selectedId = "spark";
  let zoom = 1;
  let drag = null;
  let toastTimer;

  const $ = (selector) => document.querySelector(selector);
  const canvas = $("#canvas");
  const nodesLayer = $("#nodes");
  const connections = $("#connections");
  const viewport = $("#viewport");

  function cloneDefaults() {
    return { nodes: structuredClone(defaultNodes), edges: structuredClone(defaultEdges), notes: [...defaultNotes] };
  }

  function loadState() {
    try {
      const saved = JSON.parse(localStorage.getItem(STORAGE_KEY));
      if (saved && Array.isArray(saved.nodes) && Array.isArray(saved.edges)) return saved;
    } catch (_) {}
    return cloneDefaults();
  }

  function saveState() {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(state));
    const status = $("#saveState");
    status.textContent = "Сохранено";
    status.animate([{ opacity: .35 }, { opacity: 1 }], { duration: 240 });
  }

  function escapeId(value) { return CSS.escape(value); }
  function getNode(id) { return state.nodes.find((item) => item.id === id); }

  function render() {
    renderLegend();
    renderNodes();
    renderConnections();
    renderNotes();
    renderInspector();
  }

  function renderLegend() {
    const legend = $("#legend");
    legend.replaceChildren();
    Object.entries(branches).forEach(([id, branch]) => {
      const item = document.createElement("div");
      item.className = "legend-item";
      item.style.setProperty("--branch", branch.color);
      const dot = document.createElement("span"); dot.className = "legend-dot";
      const label = document.createElement("span"); label.textContent = branch.label;
      item.append(dot, label); legend.append(item);
    });
  }

  function renderNodes() {
    nodesLayer.replaceChildren();
    state.nodes.forEach((item) => {
      const branch = branches[item.branch] || branches.root;
      const el = document.createElement("article");
      el.className = `skill-node${item.id === selectedId ? " selected" : ""}`;
      el.dataset.id = item.id;
      el.style.left = `${item.x}px`; el.style.top = `${item.y}px`; el.style.setProperty("--branch", branch.color);
      const top = document.createElement("div"); top.className = "node-top";
      const icon = document.createElement("span"); icon.className = "node-icon"; icon.textContent = branch.icon;
      const cost = document.createElement("span"); cost.className = "node-cost"; cost.textContent = item.cost ? `◆ ${item.cost}` : "СТАРТ";
      const title = document.createElement("div"); title.className = "node-title"; title.textContent = item.title;
      const description = document.createElement("div"); description.className = "node-description"; description.textContent = item.description;
      top.append(icon, cost); el.append(top, title, description);
      el.addEventListener("pointerdown", startDrag);
      nodesLayer.append(el);
    });
  }

  function renderConnections() {
    connections.replaceChildren();
    state.edges.forEach((edge) => {
      const from = getNode(edge.from); const to = getNode(edge.to);
      if (!from || !to) return;
      const line = document.createElementNS("http://www.w3.org/2000/svg", "path");
      const x1 = from.x + NODE_SIZE / 2, y1 = from.y + NODE_SIZE / 2;
      const x2 = to.x + NODE_SIZE / 2, y2 = to.y + NODE_SIZE / 2;
      line.setAttribute("d", `M ${x1} ${y1} L ${x2} ${y2}`);
      line.classList.add("connection");
      if (edge.from === selectedId || edge.to === selectedId) line.classList.add("active");
      connections.append(line);
    });
  }

  function renderNotes() {
    const list = $("#notes"); list.replaceChildren();
    state.notes.forEach((text, index) => {
      const note = document.createElement("div"); note.className = "note"; note.textContent = text;
      const remove = document.createElement("button"); remove.type = "button"; remove.textContent = "×";
      remove.addEventListener("click", () => { state.notes.splice(index, 1); saveState(); renderNotes(); });
      note.append(remove); list.append(note);
    });
  }

  function renderInspector() {
    const item = getNode(selectedId);
    $("#emptyInspector").hidden = Boolean(item);
    $("#nodeForm").hidden = !item;
    if (!item) return;
    $("#selectedLabel").textContent = item.title;
    $("#nodeTitle").value = item.title;
    $("#nodeDescription").value = item.description;
    $("#nodeCost").value = item.cost;
    const select = $("#nodeBranch"); select.replaceChildren();
    Object.entries(branches).forEach(([id, branch]) => {
      const option = document.createElement("option"); option.value = id; option.textContent = branch.label; option.selected = id === item.branch; select.append(option);
    });
    $("#deleteNode").hidden = item.id === "spark";
    const dependencies = $("#dependencies"); dependencies.replaceChildren();
    state.nodes.filter((candidate) => candidate.id !== item.id).forEach((candidate) => {
      const label = document.createElement("label"); label.className = "dependency";
      const input = document.createElement("input"); input.type = "checkbox";
      input.checked = state.edges.some((edge) => edge.from === candidate.id && edge.to === item.id);
      input.addEventListener("change", () => toggleDependency(candidate.id, item.id, input.checked));
      const text = document.createElement("span"); text.textContent = candidate.title;
      label.append(input, text); dependencies.append(label);
    });
  }

  function startDrag(event) {
    if (event.button !== 0) return;
    const id = event.currentTarget.dataset.id;
    selectedId = id;
    const item = getNode(id);
    drag = { id, startX: event.clientX, startY: event.clientY, x: item.x, y: item.y, lastX: item.x, lastY: item.y, moved: false, el: event.currentTarget };
    drag.el.setPointerCapture(event.pointerId); drag.el.classList.add("dragging", "selected");
    renderConnections(); renderInspector();
  }

  window.addEventListener("pointermove", (event) => {
    if (!drag) return;
    const dx = (event.clientX - drag.startX) / zoom, dy = (event.clientY - drag.startY) / zoom;
    if (Math.abs(dx) + Math.abs(dy) > 4) drag.moved = true;
    const x = snap(clamp(drag.x + dx, GRID_ORIGIN, canvas.offsetWidth - NODE_SIZE), GRID_X);
    const y = snap(clamp(drag.y + dy, GRID_ORIGIN, canvas.offsetHeight - NODE_SIZE), GRID_Y);
    const occupied = state.nodes.some((item) => item.id !== drag.id && item.x === x && item.y === y);
    if (!occupied) { drag.lastX = x; drag.lastY = y; }
    const item = getNode(drag.id); item.x = drag.lastX; item.y = drag.lastY;
    drag.el.style.left = `${item.x}px`; drag.el.style.top = `${item.y}px`;
    renderConnections();
  });

  window.addEventListener("pointerup", () => {
    if (!drag) return;
    drag.el.classList.remove("dragging");
    saveState(); drag = null; renderNodes();
  });

  function snap(value, step) { return GRID_ORIGIN + Math.round((value - GRID_ORIGIN) / step) * step; }
  function clamp(value, min, max) { return Math.max(min, Math.min(max, value)); }

  function toggleDependency(from, to, enabled) {
    const index = state.edges.findIndex((edge) => edge.from === from && edge.to === to);
    if (enabled && index < 0) {
      if (hasPath(to, from)) { showToast("Цикл зависимостей запрещён"); renderInspector(); return; }
      state.edges.push({ from, to });
    } else if (!enabled && index >= 0) state.edges.splice(index, 1);
    saveState(); renderConnections();
  }

  function hasPath(start, target, seen = new Set()) {
    if (start === target) return true;
    if (seen.has(start)) return false;
    seen.add(start);
    return state.edges.filter((edge) => edge.from === start).some((edge) => hasPath(edge.to, target, seen));
  }

  function updateSelected(field, value) {
    const item = getNode(selectedId); if (!item) return;
    item[field] = value;
    saveState(); renderNodes(); renderConnections();
    $("#selectedLabel").textContent = item.title;
  }

  function findFreeCell() {
    const occupied = new Set(state.nodes.map((item) => `${item.x}:${item.y}`));
    for (let radius = 0; radius < 10; radius++) {
      for (let row = Math.max(0, 4 - radius); row <= 4 + radius; row++) {
        for (let col = Math.max(0, 5 - radius); col <= 5 + radius; col++) {
          const pos = p(col, row); if (!occupied.has(`${pos.x}:${pos.y}`)) return pos;
        }
      }
    }
    return p(0, 0);
  }

  function addNode() {
    const id = `node-${Date.now()}`; const position = findFreeCell();
    state.nodes.push({ id, title: "Новая идея", description: "Опишите, как этот навык меняет поведение игрока.", branch: "mastery", cost: 1, ...position });
    selectedId = id; saveState(); render();
    setTimeout(() => $("#nodeTitle").select(), 0);
  }

  function deleteSelected() {
    if (!selectedId || selectedId === "spark") return;
    state.nodes = state.nodes.filter((item) => item.id !== selectedId);
    state.edges = state.edges.filter((edge) => edge.from !== selectedId && edge.to !== selectedId);
    selectedId = null; saveState(); render();
  }

  function resetLayout() {
    const defaults = new Map(defaultNodes.map((item) => [item.id, item]));
    const occupied = new Set();
    state.nodes.forEach((item) => {
      const initial = defaults.get(item.id);
      if (initial) { item.x = initial.x; item.y = initial.y; }
      else {
        const pos = findFreeCell(); item.x = pos.x; item.y = pos.y;
      }
      occupied.add(`${item.x}:${item.y}`);
    });
    saveState(); render(); showToast("Ноды выровнены по сетке");
  }

  function setZoom(next) {
    zoom = Math.max(.55, Math.min(1.35, next));
    canvas.style.transform = `scale(${zoom})`;
    viewport.style.setProperty("--scaled-width", `${canvas.offsetWidth * zoom}px`);
    $("#zoomValue").textContent = `${Math.round(zoom * 100)}%`;
  }

  function exportTree() {
    const blob = new Blob([JSON.stringify(state, null, 2)], { type: "application/json" });
    const link = document.createElement("a"); link.href = URL.createObjectURL(blob); link.download = "its-almost-night-skill-tree.json"; link.click();
    setTimeout(() => URL.revokeObjectURL(link.href), 0);
  }

  function importTree(file) {
    if (!file) return;
    const reader = new FileReader();
    reader.onload = () => {
      try {
        const data = JSON.parse(reader.result);
        if (!Array.isArray(data.nodes) || !Array.isArray(data.edges)) throw new Error();
        state = { nodes: data.nodes, edges: data.edges, notes: Array.isArray(data.notes) ? data.notes : [] };
        state.nodes.forEach((item) => { item.x = snap(Number(item.x) || GRID_ORIGIN, GRID_X); item.y = snap(Number(item.y) || GRID_ORIGIN, GRID_Y); });
        selectedId = state.nodes[0]?.id || null; saveState(); render(); showToast("Дерево импортировано");
      } catch (_) { showToast("Не удалось прочитать файл"); }
    };
    reader.readAsText(file);
  }

  function showToast(text) {
    const toast = $("#toast"); toast.textContent = text; toast.classList.add("visible");
    clearTimeout(toastTimer); toastTimer = setTimeout(() => toast.classList.remove("visible"), 1800);
  }

  $("#addNode").addEventListener("click", addNode);
  $("#deleteNode").addEventListener("click", deleteSelected);
  $("#resetLayout").addEventListener("click", resetLayout);
  $("#exportTree").addEventListener("click", exportTree);
  $("#importTree").addEventListener("change", (event) => importTree(event.target.files[0]));
  $("#zoomOut").addEventListener("click", () => setZoom(zoom - .1));
  $("#zoomIn").addEventListener("click", () => setZoom(zoom + .1));
  $("#zoomValue").addEventListener("click", () => setZoom(1));
  $("#nodeTitle").addEventListener("input", (event) => updateSelected("title", event.target.value));
  $("#nodeDescription").addEventListener("input", (event) => updateSelected("description", event.target.value));
  $("#nodeBranch").addEventListener("change", (event) => updateSelected("branch", event.target.value));
  $("#nodeCost").addEventListener("input", (event) => updateSelected("cost", Math.max(0, Number(event.target.value) || 0)));
  $("#addNote").addEventListener("click", () => {
    const text = prompt("Новая идея:"); if (!text?.trim()) return;
    state.notes.push(text.trim()); saveState(); renderNotes();
  });
  window.addEventListener("keydown", (event) => {
    if ((event.key === "Delete" || event.key === "Backspace") && !/INPUT|TEXTAREA|SELECT/.test(document.activeElement.tagName)) deleteSelected();
  });

  render(); setZoom(1);
  requestAnimationFrame(() => {
    const root = getNode("spark");
    viewport.scrollLeft = root.x - viewport.clientWidth / 2 + NODE_SIZE / 2;
    viewport.scrollTop = root.y - viewport.clientHeight / 2 + NODE_SIZE / 2;
  });
})();
