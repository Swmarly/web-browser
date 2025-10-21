(function () {
  const tabBar = document.getElementById('tab-bar');
  const tabContent = document.getElementById('tab-content');
  const addressBar = document.getElementById('address-bar');
  const backButton = document.getElementById('back-button');
  const forwardButton = document.getElementById('forward-button');
  const refreshButton = document.getElementById('refresh-button');
  const homeButton = document.getElementById('home-button');
  const newTabButton = document.getElementById('new-tab-button');
  const themeButton = document.getElementById('theme-button');
  const body = document.body;
  const quotePool = [
    'Level up with another cup of hot cocoa!',
    'In the world of Kona Browser, every tab is a side quest.',
    'Konata says: breaks are for pressing start on the next adventure!',
    'Stay comfy, stay curious, and keep the tabs sparkling.',
    'Otaku energy + focus mode = unstoppable browsing session.',
  ];

  const themeStorage = {
    save(theme) {
      try {
        localStorage.setItem('kona-theme', theme);
      } catch (error) {
        // Ignore storage errors (e.g. private browsing).
      }
    },
    clear() {
      try {
        localStorage.removeItem('kona-theme');
      } catch (error) {
        // Ignore storage errors.
      }
    },
    load() {
      try {
        return localStorage.getItem('kona-theme');
      } catch (error) {
        return null;
      }
    },
  };

  const KonaHomeUrl = 'about:kona';

  class TabManager {
    constructor() {
      this.tabs = new Map();
      this.activeTabId = null;
      this.counter = 0;
      this.createTab({ url: KonaHomeUrl, label: 'Home' });
      this.restoreTheme();
      this.attachGlobalListeners();
    }

    createTab({ url = KonaHomeUrl, label = 'New Tab' } = {}) {
      const id = `tab-${++this.counter}`;
      const button = document.createElement('button');
      button.type = 'button';
      button.className = 'tab';
      button.id = id;
      button.setAttribute('role', 'tab');
      button.setAttribute('aria-selected', 'false');
      button.setAttribute('tabindex', '-1');
      button.dataset.tabId = id;

      const titleSpan = document.createElement('span');
      titleSpan.className = 'tab-title';
      titleSpan.textContent = label;

      const closeButton = document.createElement('button');
      closeButton.className = 'tab-close';
      closeButton.setAttribute('type', 'button');
      closeButton.setAttribute('aria-label', 'Close tab');
      closeButton.innerHTML = '&times;';

      button.append(titleSpan, closeButton);
      tabBar.insertBefore(button, newTabButton);

      const panel = document.createElement('div');
      panel.className = 'tab-panel';
      panel.setAttribute('role', 'tabpanel');
      panel.id = `${id}-panel`;
      panel.setAttribute('aria-labelledby', id);
      button.setAttribute('aria-controls', panel.id);
      panel.setAttribute('hidden', '');

      const homeView = this.createHomeView(id);

      const iframe = document.createElement('iframe');
      iframe.className = 'tab-frame';
      iframe.setAttribute('sandbox', 'allow-scripts allow-same-origin allow-forms allow-popups');
      iframe.setAttribute('referrerpolicy', 'no-referrer-when-downgrade');
      iframe.setAttribute('loading', 'lazy');
      iframe.hidden = true;

      panel.append(homeView, iframe);
      tabContent.appendChild(panel);

      const tabInfo = {
        id,
        button,
        panel,
        iframe,
        homeView,
        titleSpan,
        history: [],
        historyIndex: -1,
        showingHome: true,
        pendingUrl: null,
        pendingEntry: null,
      };

      this.tabs.set(id, tabInfo);

      button.addEventListener('click', (event) => {
        if (event.target === closeButton) {
          event.stopPropagation();
          this.closeTab(id);
          return;
        }
        this.activateTab(id);
      });

      closeButton.addEventListener('click', (event) => {
        event.stopPropagation();
        this.closeTab(id);
      });

      iframe.addEventListener('load', () => {
        this.handleFrameLoad(id);
      });

      if (!this.activeTabId) {
        this.activateTab(id);
      }

      this.navigate(id, url, { addToHistory: true, updateTitle: true });
      return id;
    }

    activateTab(id) {
      if (!this.tabs.has(id)) {
        return;
      }

      if (this.activeTabId && this.tabs.has(this.activeTabId)) {
        const current = this.tabs.get(this.activeTabId);
        current.button.classList.remove('active');
        current.button.setAttribute('aria-selected', 'false');
        current.button.setAttribute('tabindex', '-1');
        current.panel.classList.remove('active');
        current.panel.setAttribute('hidden', '');
      }

      const next = this.tabs.get(id);
      next.button.classList.add('active');
      next.button.setAttribute('aria-selected', 'true');
      next.button.setAttribute('tabindex', '0');
      next.panel.classList.add('active');
      next.panel.removeAttribute('hidden');
      this.activeTabId = id;

      if (next.showingHome) {
        addressBar.value = KonaHomeUrl;
      } else if (next.historyIndex >= 0 && next.history[next.historyIndex]) {
        addressBar.value = next.history[next.historyIndex].display;
      } else {
        addressBar.value = next.iframe.src;
      }
      this.updateNavState();
    }

    closeTab(id) {
      const tab = this.tabs.get(id);
      if (!tab) {
        return;
      }

      const wasActive = this.activeTabId === id;
      tab.button.remove();
      tab.panel.remove();
      this.tabs.delete(id);

      if (this.tabs.size === 0) {
        this.createTab({ url: KonaHomeUrl, label: 'Home' });
        return;
      }

      if (wasActive) {
        const lastTab = Array.from(this.tabs.values()).pop();
        this.activateTab(lastTab.id);
      } else {
        this.updateNavState();
      }
    }

    navigate(id, input, options = {}) {
      const tab = this.tabs.get(id);
      if (!tab) {
        return;
      }

      const { addToHistory = false, updateTitle = false } = options;
      let normalized;
      if (typeof input === 'string') {
        normalized = this.normalizeInput(input);
      } else if (input && typeof input === 'object') {
        normalized = { ...input };
      } else {
        return;
      }

      if (normalized.url === KonaHomeUrl) {
        this.showHome(tab);
      } else {
        this.showFrame(tab, normalized.url);
      }

      if (addToHistory) {
        tab.history = tab.history.slice(0, tab.historyIndex + 1);
        const entry = { ...normalized };
        tab.history.push(entry);
        tab.historyIndex = tab.history.length - 1;
        normalized = entry;
      }

      tab.pendingEntry = normalized;

      if (tab.id === this.activeTabId) {
        addressBar.value = normalized.display;
      }

      if (updateTitle) {
        tab.titleSpan.textContent = this.getTitleFromEntry(normalized);
      }

      this.updateNavState();
    }

    navigateActive(input) {
      if (!this.activeTabId) {
        return;
      }
      this.navigate(this.activeTabId, input, { addToHistory: true, updateTitle: true });
    }

    showHome(tab) {
      tab.showingHome = true;
      tab.homeView.hidden = false;
      tab.iframe.hidden = true;
      tab.pendingUrl = null;
      tab.titleSpan.textContent = 'Home';
    }

    showFrame(tab, url) {
      tab.showingHome = false;
      tab.homeView.hidden = true;
      tab.iframe.hidden = false;
      if (tab.iframe.src !== url) {
        tab.pendingUrl = url;
        tab.iframe.src = url;
      } else {
        tab.pendingUrl = null;
      }
    }

    goBack() {
      if (!this.activeTabId) {
        return;
      }
      const tab = this.tabs.get(this.activeTabId);
      if (tab.historyIndex <= 0) {
        return;
      }
      tab.historyIndex -= 1;
      const target = tab.history[tab.historyIndex];
      this.navigate(tab.id, target, { updateTitle: true });
    }

    goForward() {
      if (!this.activeTabId) {
        return;
      }
      const tab = this.tabs.get(this.activeTabId);
      if (tab.historyIndex >= tab.history.length - 1) {
        return;
      }
      tab.historyIndex += 1;
      const target = tab.history[tab.historyIndex];
      this.navigate(tab.id, target, { updateTitle: true });
    }

    refresh() {
      if (!this.activeTabId) {
        return;
      }
      const tab = this.tabs.get(this.activeTabId);
      if (tab.showingHome) {
        this.showHome(tab);
        return;
      }
      tab.pendingUrl = tab.iframe.src;
      const currentEntry = tab.history[tab.historyIndex];
      tab.pendingEntry = currentEntry
        ? { ...currentEntry }
        : { url: tab.iframe.src, display: tab.iframe.src, kind: 'url' };
      tab.iframe.src = tab.iframe.src;
    }

    handleFrameLoad(id) {
      const tab = this.tabs.get(id);
      if (!tab || tab.showingHome) {
        return;
      }
      const currentUrl = tab.iframe.src;
      const pendingMatch = tab.pendingEntry && tab.pendingEntry.url === currentUrl;
      tab.pendingUrl = null;

      if (pendingMatch) {
        tab.pendingEntry = null;
      } else {
        tab.pendingEntry = null;
        const entry = { url: currentUrl, display: currentUrl, kind: 'url' };
        tab.history = tab.history.slice(0, tab.historyIndex + 1);
        tab.history.push(entry);
        tab.historyIndex = tab.history.length - 1;
        tab.titleSpan.textContent = this.getTitleFromEntry(entry);
        if (id === this.activeTabId) {
          addressBar.value = entry.display;
        }
      }

      if (pendingMatch && id === this.activeTabId) {
        const activeEntry = tab.history[tab.historyIndex];
        if (activeEntry) {
          addressBar.value = activeEntry.display;
          tab.titleSpan.textContent = this.getTitleFromEntry(activeEntry);
        }
      }

      this.updateNavState();
    }

    updateNavState() {
      if (!this.activeTabId) {
        backButton.disabled = true;
        forwardButton.disabled = true;
        return;
      }
      const tab = this.tabs.get(this.activeTabId);
      backButton.disabled = tab.historyIndex <= 0;
      forwardButton.disabled = tab.historyIndex === -1 || tab.historyIndex >= tab.history.length - 1;
    }

    deriveTitle(urlString) {
      try {
        const url = new URL(urlString);
        if (url.hostname) {
          return url.hostname.replace('www.', '');
        }
        return urlString;
      } catch (error) {
        return urlString;
      }
    }

    normalizeInput(input) {
      const trimmed = (input || '').trim();
      if (!trimmed || trimmed.toLowerCase() === KonaHomeUrl) {
        return { url: KonaHomeUrl, display: KonaHomeUrl, kind: 'home' };
      }

      if (/^https?:\/\//i.test(trimmed)) {
        return { url: trimmed, display: trimmed, kind: 'url' };
      }

      if (trimmed.toLowerCase().startsWith('about:kona')) {
        return { url: KonaHomeUrl, display: KonaHomeUrl, kind: 'home' };
      }

      const domainPattern = /^([\w-]+\.)+[\w-]{2,}(\/.*)?$/i;
      if (domainPattern.test(trimmed)) {
        const hasProtocol = /^[a-z]+:\/\//i.test(trimmed);
        const url = hasProtocol ? trimmed : `https://${trimmed}`;
        return { url, display: url, kind: 'url' };
      }

      const searchUrl = `https://duckduckgo.com/?q=${encodeURIComponent(trimmed)}`;
      return { url: searchUrl, display: trimmed, kind: 'search' };
    }

    getTitleFromEntry(entry) {
      if (!entry) {
        return 'Tab';
      }
      if (entry.kind === 'home') {
        return 'Home';
      }
      if (entry.kind === 'search') {
        const query = entry.display || 'Search';
        return `Search: ${this.truncateLabel(query, 24)}`;
      }
      return this.deriveTitle(entry.url);
    }

    truncateLabel(text, maxLength) {
      if (!text) {
        return '';
      }
      if (text.length <= maxLength) {
        return text;
      }
      return `${text.slice(0, Math.max(0, maxLength - 1))}…`;
    }

    createHomeView(tabId) {
      const template = document.getElementById('home-template');
      const fragment = template.content.cloneNode(true);
      const view = fragment.firstElementChild;
      const searchInput = view.querySelector('[data-action="quickSearch"]');
      const quoteElement = view.querySelector('[data-role="quote"]');

      view.querySelectorAll('[data-url]').forEach((el) => {
        el.addEventListener('click', () => {
          this.activateTab(tabId);
          this.navigate(tabId, el.dataset.url, { addToHistory: true, updateTitle: true });
        });
      });

      view.querySelectorAll('[data-action="themeToggle"]').forEach((button) => {
        button.addEventListener('click', () => {
          this.toggleTheme();
        });
      });

      if (quoteElement) {
        view.querySelectorAll('[data-action="randomQuote"]').forEach((button) => {
          button.addEventListener('click', () => {
            const quote = quotePool[Math.floor(Math.random() * quotePool.length)];
            quoteElement.textContent = quote;
            quoteElement.classList.add('pop');
            setTimeout(() => quoteElement.classList.remove('pop'), 400);
          });
        });
      }

      view.querySelectorAll('.mood-swatches button').forEach((button) => {
        button.addEventListener('click', () => {
          const theme = button.dataset.theme;
          if (theme === 'default') {
            body.removeAttribute('data-theme');
            themeStorage.clear();
          } else {
            body.setAttribute('data-theme', theme);
            themeStorage.save(theme);
          }
        });
      });

      if (searchInput) {
        searchInput.addEventListener('keydown', (event) => {
          if (event.key === 'Enter' && searchInput.value.trim()) {
            this.activateTab(tabId);
            this.navigate(tabId, searchInput.value, { addToHistory: true, updateTitle: true });
          }
        });
      }

      return view;
    }

    attachGlobalListeners() {
      addressBar.addEventListener('keydown', (event) => {
        if (event.key === 'Enter') {
          this.navigateActive(addressBar.value);
        }
      });

      refreshButton.addEventListener('click', () => this.refresh());
      backButton.addEventListener('click', () => this.goBack());
      forwardButton.addEventListener('click', () => this.goForward());
      homeButton.addEventListener('click', () => this.navigateActive(KonaHomeUrl));
      newTabButton.addEventListener('click', () => {
        const id = this.createTab({ url: KonaHomeUrl, label: 'Home' });
        this.activateTab(id);
      });

      themeButton.addEventListener('click', () => this.toggleTheme());
    }

    toggleTheme() {
      const current = body.getAttribute('data-theme');
      if (!current) {
        body.setAttribute('data-theme', 'midnight');
        themeStorage.save('midnight');
      } else if (current === 'midnight') {
        body.setAttribute('data-theme', 'sunrise');
        themeStorage.save('sunrise');
      } else if (current === 'sunrise') {
        body.setAttribute('data-theme', 'twilight');
        themeStorage.save('twilight');
      } else {
        body.removeAttribute('data-theme');
        themeStorage.clear();
      }
    }

    restoreTheme() {
      const stored = themeStorage.load();
      if (stored) {
        body.setAttribute('data-theme', stored);
      }
    }
  }

  // Enhance quotes with subtle animation class when updated via JS.
  const style = document.createElement('style');
  style.textContent = `
    .quote.pop {
      animation: quotePop 400ms ease;
    }
    @keyframes quotePop {
      0% { transform: scale(1); }
      50% { transform: scale(1.05); }
      100% { transform: scale(1); }
    }
  `;
  document.head.appendChild(style);

  window.addEventListener('DOMContentLoaded', () => {
    new TabManager();
  });
})();
