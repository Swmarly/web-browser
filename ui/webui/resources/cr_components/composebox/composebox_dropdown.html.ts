// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeboxDropdownElement} from './composebox_dropdown.js';

export function getHtml(this: ComposeboxDropdownElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  <div>
    ${this.result?.matches.map((item, index) => html`
      <ntp-composebox-match
          aria-label="${this.computeAriaLabel_(item)}"
          tabindex="0"
          role="option"
          .match="${item}"
          .matchIndex="${index}"
          ?selected="${this.isSelected_(item)}"
          ?is-last="${this.isLastMatch_(index)}"
          ?hidden="${this.hideVerbatimMatch_(index)}">
      </ntp-composebox-match>
    `)}
  </div>
  <!--_html_template_end_-->`;
  // clang-format on
}
