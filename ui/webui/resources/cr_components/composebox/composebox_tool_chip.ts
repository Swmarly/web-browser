// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './composebox_tool_chip.css.js';
import {getHtml} from './composebox_tool_chip.html.js';

declare global {
  interface HTMLElementTagNameMap {
    'composebox-tool-chip': ComposeboxToolChipElement;
  }
}

export class ComposeboxToolChipElement extends CrLitElement {
  static get is() {
    return 'composebox-tool-chip';
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      icon: {type: String},
      label: {type: String},
      visible: {type: Boolean},
    };
  }
  protected accessor icon:string = '';
  protected accessor label:string = '';
  protected accessor visible:boolean = false;

  override render() {
    if (!this.visible) {
      return;
    }
    return getHtml.call(this);
  }
}

customElements.define(ComposeboxToolChipElement.is, ComposeboxToolChipElement);
