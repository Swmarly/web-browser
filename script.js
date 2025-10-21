const doc = document.documentElement;
const tabBar = document.getElementById("tab-bar");
const tabContent = document.querySelector(".tab-content");
const tabTemplate = document.getElementById("tab-template");
const panelTemplate = document.getElementById("panel-template");
const omnibox = document.getElementById("omnibox-input");
const goButton = document.getElementById("go-button");
const newTabButton = document.getElementById("new-tab-button");
const addBookmarkButton = document.getElementById("add-bookmark");
const bookmarkTemplate = document.getElementById("bookmark-template");
const bookmarkList = document.getElementById("bookmark-list");
const toggleThemeButton = document.getElementById("toggle-theme");
const openSettingsButton = document.getElementById("open-settings");
const settingsModal = document.getElementById("settings-modal");
const sparkleToggle = document.getElementById("sparkle-toggle");
const ambientToggle = document.getElementById("ambient-sound-toggle");
const ambientAudio = document.getElementById("ambient-audio");

const STORAGE_KEYS = {
  theme: "kona-theme",
  bookmarks: "kona-bookmarks",
  tabs: "kona-tabs",
  ambient: "kona-ambient",
  sparkle: "kona-sparkle",
};

const defaultBookmarks = [
  { label: "Anime News", url: "https://www.animenewsnetwork.com/", emoji: "📰" },
  { label: "Manga Updates", url: "https://www.mangaupdates.com/", emoji: "📚" },
  { label: "Retro Games", url: "https://www.retrorgb.com/", emoji: "🕹️" },
];

let sparkleEnabled = true;
let ambientEnabled = false;
let currentTabId = "welcome";
let tabs = {
  welcome: {
    id: "welcome",
    title: "Welcome",
    src: null,
  },
};

function init() {
  restoreSettings();
  renderBookmarks();
  attachEvents();
}

function restoreSettings() {
  const storedTheme = localStorage.getItem(STORAGE_KEYS.theme);
  if (storedTheme === "day") {
    doc.setAttribute("data-theme", "day");
  }

  sparkleEnabled = localStorage.getItem(STORAGE_KEYS.sparkle) !== "false";
  ambientEnabled = localStorage.getItem(STORAGE_KEYS.ambient) === "true";

  sparkleToggle.checked = sparkleEnabled;
  ambientToggle.checked = ambientEnabled;

  if (!sparkleEnabled) {
    document.body.classList.add("no-sparkle");
  }

  if (ambientEnabled) {
    playAmbient();
  }

  const storedBookmarks = localStorage.getItem(STORAGE_KEYS.bookmarks);
  if (storedBookmarks) {
    try {
      const parsed = JSON.parse(storedBookmarks);
      if (Array.isArray(parsed)) {
        bookmarks = parsed;
      }
    } catch (err) {
      console.warn("Failed to parse bookmarks", err);
    }
  }
}

let bookmarks = [...defaultBookmarks];

function attachEvents() {
  goButton.addEventListener("click", () => handleNavigate(omnibox.value));
  omnibox.addEventListener("keydown", (event) => {
    if (event.key === "Enter") {
      event.preventDefault();
      handleNavigate(omnibox.value);
    }
  });

  newTabButton.addEventListener("click", () => {
    const newTab = createTab({
      title: "New Tab",
      src: null,
    });
    switchTab(newTab.id);
    omnibox.focus();
  });

  tabBar.addEventListener("click", (event) => {
    const tabEl = event.target.closest(".tab");
    if (!tabEl) return;

    const tabId = tabEl.dataset.tabId;
    if (event.target.matches(".tab__close")) {
      closeTab(tabId);
    } else {
      switchTab(tabId);
    }
  });

  addBookmarkButton.addEventListener("click", () => {
    const label = prompt("Bookmark name");
    if (!label) return;
    const url = prompt("Bookmark URL");
    if (!url) return;

    const bookmark = {
      label,
      url,
      emoji: "🌟",
    };
    bookmarks.push(bookmark);
    persistBookmarks();
    renderBookmarks();
  });

  bookmarkList.addEventListener("click", (event) => {
    const bookmarkEl = event.target.closest(".bookmark");
    if (!bookmarkEl) return;
    const { url } = bookmarkEl.dataset;
    handleNavigate(url);
  });

  toggleThemeButton.addEventListener("click", toggleTheme);
  openSettingsButton.addEventListener("click", () => {
    settingsModal.showModal();
  });

  sparkleToggle.addEventListener("change", (event) => {
    sparkleEnabled = event.target.checked;
    localStorage.setItem(STORAGE_KEYS.sparkle, sparkleEnabled);
    document.body.classList.toggle("no-sparkle", !sparkleEnabled);
  });

  ambientToggle.addEventListener("change", (event) => {
    ambientEnabled = event.target.checked;
    localStorage.setItem(STORAGE_KEYS.ambient, ambientEnabled);
    if (ambientEnabled) {
      playAmbient();
    } else {
      ambientAudio.pause();
      ambientAudio.currentTime = 0;
    }
  });
}

function toggleTheme() {
  const current = doc.getAttribute("data-theme");
  const next = current === "day" ? "night" : "day";
  if (next === "day") {
    doc.setAttribute("data-theme", "day");
  } else {
    doc.removeAttribute("data-theme");
  }
  localStorage.setItem(STORAGE_KEYS.theme, next === "day" ? "day" : "night");
}

function renderBookmarks() {
  bookmarkList.innerHTML = "";
  bookmarks.forEach((bookmark) => {
    const clone = bookmarkTemplate.content.firstElementChild.cloneNode(true);
    clone.dataset.url = bookmark.url;
    clone.querySelector(".bookmark__emoji").textContent = bookmark.emoji;
    clone.querySelector(".bookmark__label").textContent = bookmark.label;
    bookmarkList.appendChild(clone);
  });
}

function persistBookmarks() {
  localStorage.setItem(STORAGE_KEYS.bookmarks, JSON.stringify(bookmarks));
}

function normalizeInput(input) {
  const trimmed = input.trim();
  if (!trimmed) return null;

  if (trimmed.includes(" ")) {
    const params = new URLSearchParams({ q: trimmed });
    return `https://duckduckgo.com/?${params.toString()}`;
  }

  try {
    const url = new URL(trimmed);
    return url.href;
  } catch {
    if (/^([\w-]+\.)+[\w-]{2,}$/.test(trimmed)) {
      return `https://${trimmed}`;
    }
    const params = new URLSearchParams({ q: trimmed });
    return `https://duckduckgo.com/?${params.toString()}`;
  }
}

function handleNavigate(input) {
  const url = normalizeInput(input);
  if (!url) return;

  let tab = tabs[currentTabId];
  if (!tab || currentTabId === "welcome") {
    tab = createTab({ title: input || "New Tab", src: url });
    switchTab(tab.id);
  } else {
    tab.src = url;
    updateTabTitle(tab.id, input);
    loadTab(tab.id);
  }

  omnibox.value = url;
}

let tabCounter = 0;

function createTab({ title, src }) {
  const id = `tab-${++tabCounter}`;
  const tabData = { id, title, src };
  tabs[id] = tabData;

  const tabElement = tabTemplate.content.firstElementChild.cloneNode(true);
  tabElement.dataset.tabId = id;
  tabElement.querySelector(".tab__title").textContent = title || "New Tab";
  tabBar.appendChild(tabElement);

  const panelElement = panelTemplate.content.firstElementChild.cloneNode(true);
  panelElement.dataset.tabId = id;
  if (src) {
    panelElement.querySelector("iframe").src = src;
  }
  tabContent.appendChild(panelElement);

  return tabData;
}

function switchTab(id) {
  if (!tabs[id] && id !== "welcome") return;

  const currentTabEl = tabBar.querySelector(".tab.active");
  const currentPanel = tabContent.querySelector(".tab-panel.active");
  currentTabEl?.classList.remove("active");
  currentPanel?.classList.remove("active");

  const nextTabEl = tabBar.querySelector(`.tab[data-tab-id="${id}"]`);
  if (nextTabEl) {
    nextTabEl.classList.add("active");
  }

  const nextPanel = tabContent.querySelector(`.tab-panel[data-tab-id="${id}"]`);
  if (nextPanel) {
    nextPanel.classList.add("active");
    const iframe = nextPanel.querySelector("iframe");
    if (iframe && tabs[id]?.src) {
      iframe.src = tabs[id].src;
    }
  }

  currentTabId = id;

  if (tabs[id]?.src) {
    omnibox.value = tabs[id].src;
  } else if (id === "welcome") {
    omnibox.value = "";
  }
}

function closeTab(id) {
  if (id === "welcome") return;

  const tabEl = tabBar.querySelector(`.tab[data-tab-id="${id}"]`);
  const panelEl = tabContent.querySelector(`.tab-panel[data-tab-id="${id}"]`);
  tabEl?.remove();
  panelEl?.remove();
  delete tabs[id];

  if (currentTabId === id) {
    const remainingTabs = Object.keys(tabs);
    const lastTabId = remainingTabs[remainingTabs.length - 1] || "welcome";
    switchTab(lastTabId);
  }
}

function updateTabTitle(id, input) {
  const title = input?.trim() || "New Tab";
  tabs[id].title = title;
  const tabEl = tabBar.querySelector(`.tab[data-tab-id="${id}"]`);
  if (tabEl) {
    tabEl.querySelector(".tab__title").textContent = title;
  }
}

function loadTab(id) {
  const panelEl = tabContent.querySelector(`.tab-panel[data-tab-id="${id}"]`);
  if (!panelEl) return;
  const iframe = panelEl.querySelector("iframe");
  if (iframe) {
    iframe.src = tabs[id].src;
  }
}

function playAmbient() {
  ambientAudio.volume = 0.35;
  ambientAudio
    .play()
    .then(() => {
      /* playing */
    })
    .catch((error) => {
      console.warn("Audio playback blocked", error);
    });
}

init();
