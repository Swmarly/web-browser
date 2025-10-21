// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {SearchboxElement} from './searchbox.js';

export function getHtml(this: SearchboxElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.ntpRealboxNextEnabled ? html`
<ntp-error-scrim id="errorScrim"
    ?compact-mode="${this.realboxLayoutMode === 'Compact'}">
</ntp-error-scrim>` : nothing}
<div id="inputWrapper" @focusout="${this.onInputWrapperFocusout_}"
    @keydown="${this.onInputWrapperKeydown_}">
  <input id="input" class="truncate" type="search" autocomplete="off"
      part="searchbox-input"
      spellcheck="false" aria-live="${this.inputAriaLive_}" role="combobox"
      aria-expanded="${this.dropdownIsVisible}" aria-controls="matches"
      aria-description="${this.searchboxAriaDescription}"
      placeholder="${this.computePlaceholderText_(this.placeholderText)}"
      @copy="${this.onInputCutCopy_}"
      @cut="${this.onInputCutCopy_}" @focus="${this.onInputFocus_}"
      @input="${this.onInputInput_}" @keydown="${this.onInputKeydown_}"
      @keyup="${this.onInputKeyup_}" @mousedown="${this.onInputMouseDown_}"
      @paste="${this.onInputPaste_}">
  </input>
  <cr-searchbox-icon id="icon" .match="${this.selectedMatch_}"
      default-icon="${this.searchboxIcon_}" in-searchbox>
  </cr-searchbox-icon>
  ${this.showThumbnail ? html`
    <div id="thumbnailContainer">
      <!--Tabindex is set to 1 so that the thumbnail is tabbed first,
        then the search box. -->
      <cr-searchbox-thumbnail id="thumbnail" thumbnail-url_="${this.thumbnailUrl_}"
          ?is-deletable_="${this.isThumbnailDeletable_}"
          @remove-thumbnail-click="${this.onRemoveThumbnailClick_}"
          role="button" aria-label="${this.i18n('searchboxThumbnailLabel')}"
          tabindex="${this.getThumbnailTabindex_()}">
      </cr-searchbox-thumbnail>
    </div>
  ` : nothing}

  ${this.realboxLayoutMode.startsWith('Tall') &&
      this.composeButtonEnabled ? html`
    <cr-searchbox-compose-button id="composeButton"
        @compose-click="${this.onComposeButtonClick_}">
    </cr-searchbox-compose-button>
  ` : nothing}

  ${this.ntpRealboxNextEnabled ? html`
    <div class="dropdownContainer">
      <contextual-entrypoint-and-carousel id="context"
          part="contextual-entrypoint-and-carousel"
          exportparts="composebox-entrypoint"
          .tabSuggestions_=${this.tabSuggestions_}
          entrypoint-name="Realbox"
          @add-tab-context="${this.addTabContext_}"
          @add-file-context="${this.addFileContext_}"
          @on-file-validation-error="${this.onFileValidationError_}"
          @set-deep-search-mode="${this.setDeepSearchMode_}"
          @set-create-image-mode="${this.setCreateImageMode_}"
          @get-tab-preview="${this.getTabPreview_}"
          ?show-dropdown="${this.dropdownIsVisible}"
          realbox-layout-mode="${this.realboxLayoutMode}">
        <cr-searchbox-dropdown id="matches" part="searchbox-dropdown"
            exportparts="dropdown-content"
            role="listbox" .result="${this.result_}"
            selected-match-index="${this.selectedMatchIndex_}"
            @selected-match-index-changed="${this.onSelectedMatchIndexChanged_}"
            ?can-show-secondary-side="${this.canShowSecondarySide}"
            ?had-secondary-side="${this.hadSecondarySide}"
            @had-secondary-side-changed="${this.onHadSecondarySideChanged_}"
            ?has-secondary-side="${this.hasSecondarySide}"
            @has-secondary-side-changed="${this.onHasSecondarySideChanged_}"
            @match-focusin="${this.onMatchFocusin_}"
            @match-click="${this.onMatchClick_}"
            ?hidden="${!this.dropdownIsVisible}"
            ?show-thumbnail="${this.showThumbnail}">
        </cr-searchbox-dropdown>
      </contextual-entrypoint-and-carousel>
    </div>
  ` : html`
    <cr-searchbox-dropdown class="dropdownContainer" id="matches"
        part="searchbox-dropdown"
        exportparts="dropdown-content"
        role="listbox" .result="${this.result_}"
        selected-match-index="${this.selectedMatchIndex_}"
        @selected-match-index-changed="${this.onSelectedMatchIndexChanged_}"
        ?can-show-secondary-side="${this.canShowSecondarySide}"
        ?had-secondary-side="${this.hadSecondarySide}"
        @had-secondary-side-changed="${this.onHadSecondarySideChanged_}"
        ?has-secondary-side="${this.hasSecondarySide}"
        @has-secondary-side-changed="${this.onHasSecondarySideChanged_}"
        @match-focusin="${this.onMatchFocusin_}"
        @match-click="${this.onMatchClick_}"
        ?hidden="${!this.dropdownIsVisible}"
        ?show-thumbnail="${this.showThumbnail}">
    </cr-searchbox-dropdown>
  `}
</div>

  ${this.searchboxVoiceSearchEnabled_ ? html`
    <div class="searchbox-icon-button-container voice">
      <button id="voiceSearchButton" class="searchbox-icon-button"
          @click="${this.onVoiceSearchClick_}"
          title="${this.i18n('voiceSearchButtonLabel')}">
      </button>
    </div>
  ` : nothing}

  ${this.searchboxLensSearchEnabled_ ? html`
    <div class="searchbox-icon-button-container lens">
      <button id="lensSearchButton" class="searchbox-icon-button lens"
          @click="${this.onLensSearchClick_}"
          title="${this.i18n('lensSearchButtonLabel')}">
      </button>
    </div>
  ` : nothing}

  ${!this.realboxLayoutMode.startsWith('Tall') &&
      this.composeButtonEnabled ? html`
    <cr-searchbox-compose-button id="composeButton"
        @compose-click="${this.onComposeButtonClick_}">
    </cr-searchbox-compose-button>
  ` : nothing}

<!--_html_template_end_-->`;
  // clang-format on
}
