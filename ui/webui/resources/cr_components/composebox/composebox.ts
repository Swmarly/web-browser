// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './composebox_tool_chip.js';
import './context_menu_entrypoint.js';
import './contextual_entrypoint_and_carousel.js';
import './recent_tab_chip.js';
import './composebox_dropdown.js';
import './error_scrim.js';
import './file_carousel.js';
import './icons.html.js';
import '//resources/cr_components/localized_link/localized_link.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {getInstance as getAnnouncerInstance} from '//resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {hasKeyModifiers} from '//resources/js/util.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {AutocompleteMatch, AutocompleteResult, PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import type {ComposeboxFile} from './common.js';
import {getCss} from './composebox.css.js';
import {getHtml} from './composebox.html.js';
import type {PageHandlerRemote} from './composebox.mojom-webui.js';
import type {ComposeboxDropdownElement} from './composebox_dropdown.js';
import {ComposeboxProxyImpl} from './composebox_proxy.js';
import type {FileUploadErrorType} from './composebox_query.mojom-webui.js';
import {FileUploadStatus} from './composebox_query.mojom-webui.js';
import type {ContextualEntrypointAndCarouselElement} from './contextual_entrypoint_and_carousel.js';
import {ComposeboxMode} from './contextual_entrypoint_and_carousel.js';
import type {ErrorScrimElement} from './error_scrim.js';

export interface ComposeboxElement {
  $: {
    cancelIcon: CrIconButtonElement,
    input: HTMLInputElement,
    composebox: HTMLElement,
    submitIcon: CrIconButtonElement,
    matches: ComposeboxDropdownElement,
    context: ContextualEntrypointAndCarouselElement,
    errorScrim: ErrorScrimElement,
  };
}

export class ComposeboxElement extends I18nMixinLit
(CrLitElement) {
  static get is() {
    return 'ntp-composebox';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      input_: {type: String},
      isCollapsible: {
        reflect: true,
        type: Boolean,
      },
      expanded_: {
        reflect: true,
        type: Boolean,
      },
      result_: {type: Object},
      submitEnabled_: {
        reflect: true,
        type: Boolean,
      },
      /**
       * Index of the currently selected match, if any.
       * Do not modify this. Use <composebox-dropdown> API to change
       * selection.
       */
      selectedMatchIndex_: {type: Number},
      submitting_: {
        reflect: true,
        type: Boolean,
      },
      showDropdown_: {
        reflect: true,
        type: Boolean,
      },
      showSubmit_: {
        reflect: true,
        type: Boolean,
      },
      enableImageContextualSuggestions_: {
        reflect: true,
        type: Boolean,
      },
      inputPlaceholder_: {
        reflect: true,
        type: String,
      },
      smartComposeEnabled_: {
        reflect: true,
        type: Boolean,
      },
      smartComposeInlineHint_: {type: String},
      showFileCarousel_: {
        reflect: true,
        type: Boolean,
      },
      inDeepSearchMode_: {
        reflect: true,
        type: Boolean,
      },
      inCreateImageMode_: {
        reflect: true,
        type: Boolean,
      },
      showContextMenuDescription_: {type: Boolean},
      inputsDisabled_: {
        reflect: true,
        type: Boolean,
      },
      lensButtonDisabled_: {
        reflect: true,
        type: Boolean,
      },
      ntpRealboxNextEnabled: {
        type: Boolean,
        reflect: true,
      },
      tabSuggestions_: {type: Array},
      errorScrimVisible_: {type: Boolean},
      contextFilesSize_: {type: Number},
      realboxLayoutMode: {
        type: String,
        reflect: true,
      },
    };
  }

  accessor ntpRealboxNextEnabled: boolean = false;
  accessor realboxLayoutMode: string = '';
  // If isCollapsible is set to true, the composebox will be a pill shape until
  // it gets focused, at which point it will expand. If false, defaults to the
  // expanded state.
  protected accessor isCollapsible: boolean = false;
  // Whether the composebox is currently expanded. Always true if isCollapsible
  // is false.
  protected accessor expanded_: boolean = false;
  protected accessor input_: string = '';
  protected accessor showDropdown_: boolean =
      loadTimeData.getBoolean('composeboxShowZps');
  protected accessor showSubmit_: boolean =
      loadTimeData.getBoolean('composeboxShowSubmit');
  protected accessor enableImageContextualSuggestions_: boolean =
      loadTimeData.getBoolean('composeboxShowImageSuggest');
  // When enabled, the file input buttons will not be rendered.
  protected accessor selectedMatchIndex_: number = -1;
  protected accessor submitting_: boolean = false;
  protected accessor submitEnabled_: boolean = false;
  protected accessor result_: AutocompleteResult|null = null;
  protected accessor smartComposeInlineHint_: string = '';
  protected accessor smartComposeEnabled_: boolean =
      loadTimeData.getBoolean('composeboxSmartComposeEnabled');
  protected accessor inputPlaceholder_: string =
      loadTimeData.getString('searchboxComposePlaceholder');
  protected accessor showFileCarousel_: boolean = false;
  protected accessor inCreateImageMode_: boolean = false;
  protected accessor inDeepSearchMode_: boolean = false;
  protected accessor showContextMenuDescription_: boolean = true;
  protected accessor inputsDisabled_: boolean = false;
  protected accessor lensButtonDisabled_: boolean = false;
  protected accessor tabSuggestions_: TabInfo[] = [];
  protected accessor errorScrimVisible_: boolean = false;
  protected accessor contextFilesSize_: number = 0;
  protected lastQueriedInput_: string = '';
  private showTypedSuggest_: boolean =
      loadTimeData.getBoolean('composeboxShowTypedSuggest');
  private showZps: boolean = loadTimeData.getBoolean('composeboxShowZps');
  private browserProxy: ComposeboxProxyImpl = ComposeboxProxyImpl.getInstance();
  private searchboxCallbackRouter_: SearchboxPageCallbackRouter;
  private pageHandler_: PageHandlerRemote;
  private searchboxHandler_: SearchboxPageHandlerRemote;
  private eventTracker_: EventTracker = new EventTracker();
  private searchboxListenerIds: number[] = [];
  private composeboxCloseByEscape_: boolean =
      loadTimeData.getBoolean('composeboxCloseByEscape');

  private selectedMatch_: AutocompleteMatch|null = null;

  constructor() {
    super();
    this.pageHandler_ = ComposeboxProxyImpl.getInstance().handler;
    this.searchboxCallbackRouter_ =
        ComposeboxProxyImpl.getInstance().searchboxCallbackRouter;
    this.searchboxHandler_ = ComposeboxProxyImpl.getInstance().searchboxHandler;
  }

  override connectedCallback() {
    super.connectedCallback();

    // Set the initial expanded state based on the inputted property.
    this.expanded_ = !this.isCollapsible;

    this.searchboxListenerIds = [
      this.searchboxCallbackRouter_.autocompleteResultChanged.addListener(
          this.onAutocompleteResultChanged_.bind(this)),
      this.searchboxCallbackRouter_.onContextualInputStatusChanged.addListener(
          this.onContextualInputStatusChanged_.bind(this)),
      this.searchboxCallbackRouter_.onTabStripChanged.addListener(
          this.refreshTabSuggestions_.bind(this)),
    ];

    this.eventTracker_.add(this.$.input, 'input', () => {
      this.submitEnabled_ = this.computeSubmitEnabled_();
    });
    this.eventTracker_.add(this.$.context, 'on-context-files-changed',
        (e: CustomEvent<{files: number}>) => {
          this.contextFilesSize_ = e.detail.files;
          this.submitEnabled_ = this.computeSubmitEnabled_();
        });
    this.$.input.focus();
    // For realbox next, the zps autocomplete query is triggered after
    // the state has been initialized.
    if (this.showZps && !this.ntpRealboxNextEnabled) {
      this.queryAutocomplete(/* clearMatches= */ false);
    }

    this.searchboxHandler_.notifySessionStarted();
    this.refreshTabSuggestions_();

    if (this.ntpRealboxNextEnabled) {
      this.fire('composebox-initialized', {
        initializeComposeboxState: this.initializeState_.bind(this),
      });
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.searchboxHandler_.notifySessionAbandoned();

    this.searchboxListenerIds.forEach(
        id => assert(
            this.browserProxy.searchboxCallbackRouter.removeListener(id)));
    this.searchboxListenerIds = [];

    this.eventTracker_.removeAll();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    let showDropdownUpdated = changedPrivateProperties.has('showDropdown_');
    // When the result initially gets set check if dropdown should show.
    if (changedPrivateProperties.has('input_') ||
        changedPrivateProperties.has('result_') ||
        changedPrivateProperties.has('contextFilesSize_') ||
        changedPrivateProperties.has('errorScrimVisible_')) {
      const prevValue = this.showDropdown_;
      this.showDropdown_ = this.computeShowDropdown_();
      showDropdownUpdated ||= this.showDropdown_ !== prevValue;
    }
    if (this.ntpRealboxNextEnabled && showDropdownUpdated) {
      this.dispatchEvent(
          new CustomEvent('composebox-dropdown-visible-changed', {
            bubbles: true,
            composed: true,
            detail: {value: this.showDropdown_},
          }));
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('selectedMatchIndex_')) {
      if (this.selectedMatch_) {
        // If the selected match is the default match (typing) the input will
        // already have been set by handleInput.
        if (!(this.selectedMatchIndex_ === 0 &&
            this.selectedMatch_.allowedToBeDefaultMatch)) {
          // Update the input.
          const text = this.selectedMatch_.fillIntoEdit;
          assert(text);
          this.input_ = text;
          this.submitEnabled_ = true;
        }
      } else if (!this.lastQueriedInput_) {
        // This is for cases when focus leaves the matches/input.
        // If there was already text in the input do not clear it.
        this.input_ = '';
        this.submitEnabled_ = false;
      } else {
        // For typed queries reset the input back to typed value when
        // focus leaves the match.
        this.input_ = this.lastQueriedInput_;
      }
    }
    if (changedPrivateProperties.has('smartComposeInlineHint_')) {
      if (this.smartComposeInlineHint_) {
        this.adjustInputForSmartCompose();
        // TODO(crbug.com/452619068): Investigate why screenreader is
        // inconsistent.
        const announcer = getAnnouncerInstance();
        announcer.announce(
            this.smartComposeInlineHint_ + ', ' +
            this.i18n('composeboxSmartComposeTitle'));
      } else {
        // Unset the height override so input can expand through typing.
        this.$.input.style.height =
            'calc-size(fit-content, min(size + 4px, 190px))';
      }
    }
  }

  getText() {
    return this.input_;
  }

  setText(text: string) {
    this.input_ = text;
  }

  resetModes() {
    this.$.context.resetModes();
  }

  closeDropdown() {
    this.clearAutocompleteMatches_();
  }

  getSmartComposeForTesting() {
    return this.smartComposeInlineHint_;
  }

  protected initializeState_(text: string = '', files: ComposeboxFile[] = [],
                             mode: ComposeboxMode = ComposeboxMode.DEFAULT) {
    if (text) {
      this.input_ = text;
      this.lastQueriedInput_ = text;
    }
    if (this.showZps && files.length === 0) {
      this.queryAutocomplete(/* clearMatches= */ false);
    }
    if (files.length > 0) {
      this.$.context.setContextFiles(files);
    }
    if (mode !== ComposeboxMode.DEFAULT) {
      this.$.context.setInitialMode(mode);
    }
  }

  protected computeCancelButtonTitle_() {
    return this.input_.trim().length > 0 || this.contextFilesSize_ > 0 ?
        this.i18n('composeboxCancelButtonTitleInput') :
        this.i18n('composeboxCancelButtonTitle');
  }

  private computeShowDropdown_() {
    // Don't show dropdown if there's multiple files.
    if (this.contextFilesSize_ > 1) {
      return false;
    }

    // Don't show dropdown if there's no results.
    if (!this.result_?.matches.length) {
      return false;
    }

    // Do not show dropdown if there's an error scrim.
    if (this.errorScrimVisible_) {
      return false;
    }

    if (this.showTypedSuggest_ && this.input_.trim()) {
      // Do not show dropdown for multiline input.
      if (this.$.input.scrollHeight <= 48) {
        return true;
      }
    }

    // lastQueriedInput_ is used here since the input_ changes based on
    // the selected match. If typed suggest is not enabled and input_ is used,
    // the dropdown will hide if the user keys down over zps matches.
    return this.showZps && !this.lastQueriedInput_;
  }

  private computeSubmitEnabled_() {
    return this.input_.trim().length > 0 || this.contextFilesSize_ > 0;
  }

  protected shouldShowSuggestionActivityLink_() {
    if (!this.result_ || !this.showDropdown_) {
      return false;
    }
    return this.result_.matches.some((match) => match.isNoncannedAimSuggestion);
  }

  protected shouldShowSmartComposeInlineHint_() {
    return !!this.smartComposeInlineHint_;
  }

  protected onFileValidationError_(e: CustomEvent<{errorMessage: string}>) {
    this.$.errorScrim.setErrorMessage(e.detail.errorMessage);
  }

  protected async deleteContext_(e: CustomEvent<{uuid: UnguessableToken}>) {
    // If we're in create image mode, notify that image is gone.
    if (this.inCreateImageMode_) {
      await this.setCreateImageMode_({
        detail: {
          inCreateImageMode: true,
          imagePresent: this.$.context.hasImageFiles(),
        },
      } as CustomEvent<{inCreateImageMode: boolean, imagePresent: boolean}>);
    }
    this.searchboxHandler_.deleteContext(e.detail.uuid);
    this.$.input.focus();
    this.queryAutocomplete(/* clearMatches= */ true);
  }

  protected async addFileContext_(e: CustomEvent<{
      files: File[], isImage: boolean,
      onContextAdded: (files: Map<UnguessableToken, ComposeboxFile>) => void,
  }>) {
    const composeboxFiles: Map<UnguessableToken, ComposeboxFile> = new Map();
    for (const file of e.detail.files) {
      const fileBuffer = await file.arrayBuffer();
      const bigBuffer:
            BigBuffer = {bytes: Array.from(new Uint8Array(fileBuffer))};
      const {token} = await this.searchboxHandler_.addFileContext(
          {
            fileName: file.name,
            mimeType: file.type,
            selectionTime: new Date(),
          },
          bigBuffer);

      const attachment: ComposeboxFile = {
          uuid: token,
          name: file.name,
          objectUrl: e.detail.isImage ? URL.createObjectURL(file) : null,
          type: file.type,
          status: FileUploadStatus.kNotUploaded,
          url: null,
          file: file,
          tabId: null,
        };
      composeboxFiles.set(token, attachment);
      const announcer = getAnnouncerInstance();
      announcer.announce(this.i18n('composeboxFileUploadStartedText'));
    }
    e.detail.onContextAdded(composeboxFiles);
    this.$.input.focus();
  }

  protected async addTabContext_(e: CustomEvent<{
      id: number, title: string, url: Url,
      onContextAdded: (file: ComposeboxFile) => void,
  }>) {
    const {token} = await this.searchboxHandler_.addTabContext(e.detail.id);
    if (!token) {
      return;
    }

    const attachment: ComposeboxFile = {
      uuid: token,
      name: e.detail.title,
      objectUrl: null,
      type: 'tab',
      status: FileUploadStatus.kNotUploaded,
      url: e.detail.url,
      file: null,
      tabId: e.detail.id,
    };
    e.detail.onContextAdded(attachment);
    this.$.input.focus();
  }

  protected async refreshTabSuggestions_() {
    const {tabs} = await this.searchboxHandler_.getRecentTabs();
    this.tabSuggestions_ = [...tabs];
  }

  protected async getTabPreview_(e: CustomEvent<{
    tabId: number,
    onPreviewFetched: (previewDataUrl: string) => void,
  }>) {
    const {previewDataUrl} =
        await this.searchboxHandler_.getTabPreview(e.detail.tabId);
    e.detail.onPreviewFetched(previewDataUrl || '');
  }

  protected onCancelClick_() {
    if (this.input_.trim().length > 0 || this.contextFilesSize_ > 0) {
      this.input_ = '';
      this.$.context.resetContextFiles();
      this.contextFilesSize_ = 0;
      this.smartComposeInlineHint_ = '';
      this.submitEnabled_ = false;
      this.searchboxHandler_.clearFiles();
      this.$.input.focus();
      this.queryAutocomplete(/* clearMatches= */ true);
    } else {
      this.closeComposebox_();
    }
  }

  protected onLensClick_() {
    this.pageHandler_.handleLensButtonClick();
  }

  protected onLensIconMouseDown_(e: MouseEvent) {
    // Prevent the composebox from expanding due to being focused by capturing
    // the mousedown event. This is needed to allow the Lens icon to be
    // clicked when the composebox does not have focus without expanding the
    // composebox.
    e.preventDefault();
  }

  private updateInputPlaceholder_() {
    if (this.inDeepSearchMode_) {
      this.inputPlaceholder_ =
          loadTimeData.getString('composeDeepSearchPlaceholder');
    } else if (this.inCreateImageMode_) {
      this.inputPlaceholder_ =
          loadTimeData.getString('composeCreateImagePlaceholder');
    } else {
      this.inputPlaceholder_ =
          loadTimeData.getString('searchboxComposePlaceholder');
    }
  }

  protected async setDeepSearchMode_(
      e: CustomEvent<{inDeepSearchMode: boolean}>) {
    this.inDeepSearchMode_ = e.detail.inDeepSearchMode;
    this.pageHandler_.setDeepSearchMode(e.detail.inDeepSearchMode);
    this.queryAutocomplete(/* clearMatches= */ true);
    this.updateInputPlaceholder_();

    await this.updateComplete;
    this.$.input.focus();
  }

  protected async setCreateImageMode_(
      e: CustomEvent<{inCreateImageMode: boolean, imagePresent: boolean}>) {
    this.inCreateImageMode_ = e.detail.inCreateImageMode;
    this.pageHandler_.setCreateImageMode(
        e.detail.inCreateImageMode, e.detail.imagePresent);
    this.queryAutocomplete(/* clearMatches= */ true);
    this.updateInputPlaceholder_();

    await this.updateComplete;
    this.$.input.focus();
  }

  protected onErrorScrimVisibilityChanged_(
      e: CustomEvent<{showErrorScrim: boolean}>) {
    this.errorScrimVisible_ = e.detail.showErrorScrim;
  }

  // Sets the input property to compute the cancel button title without using
  // "$." syntax  as this is not allowed in WillUpdate().
  protected handleInput_(e: Event) {
    const inputElement = e.target as HTMLInputElement;
    this.input_ = inputElement.value;
    // `clearMatches` is true if input is empty stop any in progress providers
    // before requerying for on-focus (zero-suggest) inputs. The searchbox
    // doesn't allow zero-suggest requests to be made while the ACController is
    // not done.
    this.queryAutocomplete(/* clearMatches= */ this.input_ === '');
  }

  protected onKeydown_(e: KeyboardEvent) {
    const KEYDOWN_HANDLED_KEYS = [
      'ArrowDown',
      'ArrowUp',
      'Enter',
      'Escape',
      'PageDown',
      'PageUp',
      'Tab',
    ];

    if (!KEYDOWN_HANDLED_KEYS.includes(e.key)) {
      return;
    }

    if (this.shadowRoot.activeElement === this.$.input) {
      if ((e.key === 'ArrowDown' || e.key === 'ArrowUp') &&
          !this.showDropdown_) {
        return;
      }

      if (e.key === 'Tab') {
        // If focus leaves the input, unselect the first match.
        if (e.shiftKey) {
          this.$.matches.unselect();
        } else if (this.smartComposeEnabled_ && this.smartComposeInlineHint_) {
          this.input_ = this.input_ + this.smartComposeInlineHint_;
          this.smartComposeInlineHint_ = '';
          e.preventDefault();
          this.queryAutocomplete(/* clearMatches= */ true);
        }
        return;
      }
    }

    if (e.key === 'Enter' && this.submitEnabled_) {
      if (this.shadowRoot.activeElement === this.$.matches || !e.shiftKey) {
        e.preventDefault();
        this.submitQuery_(e);
      }
    }

    if (e.key === 'Escape' && this.composeboxCloseByEscape_) {
      this.closeComposebox_();
      e.preventDefault();
      return;
    }

    // Do not handle the following keys if there are no matches available.
    if (!this.result_ || this.result_.matches.length === 0) {
      return;
    }

    // Do not handle the following keys if there are key modifiers.
    if (hasKeyModifiers(e)) {
      return;
    }

    if (e.key === 'ArrowDown') {
      this.$.matches.selectNext();
    } else if (e.key === 'ArrowUp') {
      this.$.matches.selectPrevious();
    } else if (e.key === 'Escape' || e.key === 'PageUp') {
      this.$.matches.selectFirst();
    } else if (e.key === 'PageDown') {
      this.$.matches.selectLast();
    } else if (e.key === 'Tab') {
      // If focus goes past the last match, unselect the last match.
      if (this.selectedMatchIndex_ === this.result_.matches.length - 1) {
        const focusedMatchElem =
            this.shadowRoot.activeElement?.shadowRoot?.activeElement;
        const focusedButtonElem = focusedMatchElem?.shadowRoot?.activeElement;
        if (focusedButtonElem?.id === 'remove') {
          this.$.matches.unselect();
        }
      }
      return;
    }
    this.smartComposeInlineHint_ = '';
    e.preventDefault();

    // Focus the selected match if focus is currently in the matches.
    if (this.shadowRoot.activeElement === this.$.matches) {
      this.$.matches.focusSelected();
    }
  }

  protected handleInputFocusIn_() {
    // if there's a last queried input, it's guaranteed that at least
    // the verbatim match will exist.
    if (this.lastQueriedInput_ && this.result_?.matches.length) {
      this.$.matches.selectFirst();
    }
    if (this.ntpRealboxNextEnabled) {
      this.fire('composebox-input-focus-changed', {value: true});
    }
  }

  protected handleInputFocusOut_() {
    if (this.ntpRealboxNextEnabled) {
      this.fire('composebox-input-focus-changed', {value: false});
    }
  }

  protected handleComposeboxFocusIn_(e: FocusEvent) {
    // Exit early if the focus is still within the composebox.
    if (this.$.composebox.contains(e.relatedTarget as Node)) {
      return;
    }
    this.expanded_ = true;
    this.submitting_ = false;
    this.pageHandler_.focusChanged(true);
    this.fire('composebox-focus-in');
  }

  protected handleComposeboxFocusOut_(e: FocusEvent) {
    // Exit early if the focus is still within the composebox.
    if (this.$.composebox.contains(e.relatedTarget as Node)) {
      return;
    }
    // If the input is blurred and the composebox is expandable, collapse it.
    // Else, keep the composebox expanded.
    this.expanded_ = !this.isCollapsible;
    this.pageHandler_.focusChanged(false);
    this.fire('composebox-focus-out');
  }

  protected handleScroll_() {
    const smartCompose =
        this.shadowRoot.querySelector<HTMLElement>('#smartCompose');
    if (!smartCompose) {
      return;
    }
    smartCompose.scrollTop = this.$.input.scrollTop;
  }

  protected handleSubmitFocusIn_() {
    // Matches should always be greater than 0 due to verbatim match.
    if (this.input_ && !this.selectedMatch_) {
      this.$.matches.selectFirst();
    }
  }

  private closeComposebox_() {
    this.resetModes();
    this.fire('close-composebox', {composeboxText: this.input_});

    if (this.isCollapsible) {
      this.expanded_ = false;
      this.$.input.blur();
    }
  }

  protected submitQuery_(e: KeyboardEvent|MouseEvent) {
    // Users are allowed to submit queries that consist of only files with no
    // input. `selectedMatchIndex_` will be >= 0 when there is non-empty input
    // since the verbatim match is present.
    assert(
        (this.selectedMatchIndex_ >= 0 && this.result_) ||
         this.contextFilesSize_ > 0);

    // If there is a match that is selected, open that match, else follow the
    // non-autocomplete submission flow. The non-autocomplete submission flow
    // will not have omnibox metrics recorded for it.
    if (this.selectedMatchIndex_ >= 0) {
      const match = this.result_!.matches[this.selectedMatchIndex_];
      assert(match);
      this.searchboxHandler_.openAutocompleteMatch(
          this.selectedMatchIndex_, match.destinationUrl,
          /* are_matches_showing */ true, (e as MouseEvent).button || 0,
          e.altKey, e.ctrlKey, e.metaKey, e.shiftKey);
    } else {
      this.searchboxHandler_.submitQuery(
          this.input_.trim(), (e as MouseEvent).button || 0, e.altKey,
          e.ctrlKey, e.metaKey, e.shiftKey);
    }

    this.submitting_ = true;

    // If the composebox is expandable, collapse it and clear the input after
    // submitting.
    if (this.isCollapsible) {
      this.setText('');
      this.$.input.blur();
      this.submitEnabled_ = false;
    }
  }

  /**
   * @param e Event containing index of the match that received focus.
   */
  protected onMatchFocusin_(e: CustomEvent<{index: number}>) {
    // Select the match that received focus.
    this.$.matches.selectIndex(e.detail.index);
  }

  protected onMatchClick_() {
    this.clearAutocompleteMatches_();
  }

  protected onSelectedMatchIndexChanged_(e: CustomEvent<{value: number}>) {
    this.selectedMatchIndex_ = e.detail.value;
    this.selectedMatch_ =
        this.result_?.matches[this.selectedMatchIndex_] || null;
  }

  /**
   * Clears the autocomplete result on the page and on the autocomplete backend.
   */
  private clearAutocompleteMatches_() {
    this.showDropdown_ = false;
    this.result_ = null;
    this.$.matches.unselect();
    this.searchboxHandler_.stopAutocomplete(/*clearResult=*/ true);
    // Autocomplete sends updates once it is stopped. Invalidate those results
    // by setting the |this.lastQueriedInput_| to its default value.
    this.lastQueriedInput_ = '';
  }

  private onAutocompleteResultChanged_(result: AutocompleteResult) {
    if (this.lastQueriedInput_ === null ||
        this.lastQueriedInput_.trimStart() !== result.input) {
      return;
    }
    this.result_ = result;
    const hasMatches = this.result_.matches.length > 0;
    const firstMatch = hasMatches ? this.result_.matches[0] : null;
    // Zero suggest matches are not allowed to be default. Therefore, this
    // makes sure zero suggest results aren't focused when they are returned.
    if (firstMatch && firstMatch.allowedToBeDefaultMatch) {
      this.$.matches.selectFirst();
    } else if (
        this.input_.trim() && hasMatches && this.selectedMatchIndex_ >= 0 &&
        this.selectedMatchIndex_ < this.result_.matches.length) {
      // Restore the selection and update the input. Don't restore when the
      // user deletes all their input and autocomplete is queried or else the
      // empty input will change to the value of the first result.
      this.$.matches.selectIndex(this.selectedMatchIndex_);

      // Set the selected match since the `selectedMatchIndex_` does not change
      // (and therefore `selectedMatch_` does not get updated since
      // `onSelectedMatchIndexChanged_` is not called).
      this.selectedMatch_ = this.result_.matches[this.selectedMatchIndex_]!;
      this.input_ = this.selectedMatch_.fillIntoEdit;
    } else {
      this.$.matches.unselect();
    }

    // Populate the smart compose suggestion.
    this.smartComposeInlineHint_ = this.result_.smartComposeInlineHint ?
        this.result_.smartComposeInlineHint :
        '';
  }

  private async onContextualInputStatusChanged_(
      token: UnguessableToken, status: FileUploadStatus,
      errorType: FileUploadErrorType) {
    const {file, errorMessage} =
        this.$.context.updateFileStatus(token, status, errorType);
    if (errorMessage) {
      this.$.errorScrim.setErrorMessage(errorMessage);
    } else if (file) {
      if (status === FileUploadStatus.kProcessingSuggestSignalsReady &&
          this.showZps &&
          !file.type.includes('image')) {
        // Query autocomplete to get contextual suggestions for files.
        this.queryAutocomplete(/* clearMatches= */ true);
      }

      if (status === FileUploadStatus.kProcessingSuggestSignalsReady &&
          file.type.includes('image')) {
        // If we're in create image mode, update the aim tool mode.
        if (this.inCreateImageMode_) {
          await this.setCreateImageMode_(
              {
                detail: {
                  inCreateImageMode: true,
                  imagePresent: true,
                },
              } as
              CustomEvent<{inCreateImageMode: boolean, imagePresent: boolean}>);
        } else if (this.enableImageContextualSuggestions_) {
          // Query autocomplete to get contextual suggestions for files.
          this.queryAutocomplete(/* clearMatches= */ true);
        } else {
          this.showDropdown_ = false;
          this.clearAutocompleteMatches_();
        }
      }

      if (status === FileUploadStatus.kUploadSuccessful) {
        const announcer = getAnnouncerInstance();
        announcer.announce(this.i18n('composeboxFileUploadCompleteText'));
      }
    }
  }

  private adjustInputForSmartCompose() {
    // Checks the scroll height of the input + smart complete hint (ghost div)
    // and updates the height of the actual input to be that height so the
    // ghost text does not overflow.
    const smartCompose =
        this.shadowRoot.querySelector<HTMLElement>('#smartCompose');

    const ghostHeight = smartCompose!.scrollHeight;
    const maxHeight = 190;
    this.$.input.style.height = `${Math.min(ghostHeight, maxHeight)}px`;

    // If the height of the input + smart complete hint is greater than the max
    // height, scroll the smart compose as the input will already scroll. Note
    // there is an issue at the break point since the input will not have
    // scrolled yet as it does not have enough content. The smart compose will
    // display the ghost text below the input and it will be cut off. However,
    // the current response only works for queries below the max height.
    if (ghostHeight > maxHeight) {
      smartCompose!.scrollTop = this.$.input.scrollTop;
    }
  }

  // `queryAutocomplete` updates the `lastQueriedInput_` and makes an
  // autocomplete call through the handler. It also optionally clears existing
  // matches.
  private queryAutocomplete(clearMatches: boolean) {
    if (clearMatches) {
      this.clearAutocompleteMatches_();
    }
    this.lastQueriedInput_ = this.input_;
    this.searchboxHandler_.queryAutocomplete(this.input_, false);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-composebox': ComposeboxElement;
  }
}

customElements.define(ComposeboxElement.is, ComposeboxElement);
