// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Sentence} from './read_aloud_types.js';

// Wrapper class for Intl.Segmenter that manages Intl.Segmenter instances to
// be used to segment text.
export class TextSegmenter {
  private wordSegmenter_!: Intl.Segmenter;
  private sentenceSegmenter_!: Intl.Segmenter;

  constructor() {
    // If no language code has been provided, Intl.Segmenter will use the system
    // default language.
    this.updateLanguage();
  }

  updateLanguage(lang?: string) {
    // The try-catch is needed because Intl.Segmenter throws an error if the
    // language code is not well-formed.
    try {
      this.wordSegmenter_ = new Intl.Segmenter(lang, {granularity: 'word'});
      this.sentenceSegmenter_ =
          new Intl.Segmenter(lang, {granularity: 'sentence'});
    } catch {
      this.wordSegmenter_ =
          new Intl.Segmenter(undefined, {granularity: 'word'});
      this.sentenceSegmenter_ =
          new Intl.Segmenter(undefined, {granularity: 'sentence'});
    }
  }

  getWordCount(text: string): number {
    return Array.from(this.wordSegmenter_.segment(text))
        .filter(segment => segment.isWordLike)
        .length;
  }


  // Returns the end index of the first word in the given segment of text.
  // If there are no words, returns 0.
  getNextWordEnd(text: string): number {
    try {
      const segments = this.wordSegmenter_.segment(text);
      for (const segment of segments) {
        if (segment.isWordLike) {
          return segment.index + segment.segment.length;
        }
      }
    } catch (e) {
      // Intl.Segmenter may throw an error for invalid locales.
    }
    return 0;
  }


  getSentences(text: string): Sentence[] {
    const segments = this.sentenceSegmenter_.segment(text);
    // TODO: crbug.com/440400392- Filter out "sentences" that are just
    // punctuation.
    // Map the iterable returned by Intl.Segmenter.segment to the Sentence
    // custom type.
    return Array.from(
        segments, (segment) => ({text: segment.segment, index: segment.index}));
  }

  getAccessibleBoundary(text: string, maxTextLength: number): number {
    // If there's a viable sentence boundary within the maxTextLength, use that.
    const shorterString = text.slice(0, maxTextLength);
    const sentenceEndsShort = this.getSentenceEnd(shorterString);
    const sentenceEndsLong = this.getSentenceEnd(text);

    // Compare the index result for the sentence of maximum text length and of
    // the longer text string. If the two values are the same, the index is
    // correct. If they are different, the maximum text length may have
    // incorrectly spliced a word (e.g. returned "this is a sen" instead of
    // "this is a" or "this is a sentence"), so if this is the case, we'll want
    // to use the last word boundary instead.
    if (sentenceEndsShort === sentenceEndsLong) {
      return sentenceEndsShort;
    }

    // Fallback to word boundaries if there's no viable sentence boundary within
    // the maxTextLength. This may result in choppy speech but this is
    // preferable to cutting off speech in the middle of a word.
    try {
      const wordSegments =
          Array.from(this.wordSegmenter_.segment(shorterString));

      // Find the start of the last word.
      for (let i = wordSegments.length - 1; i >= 0; i--) {
        if (wordSegments[i]?.isWordLike) {
          return wordSegments[i]!.index;
        }
      }
    } catch (e) {
      // Intl.Segmenter may throw an error for invalid locales.
      // Fall through to return 0.
    }


    return 0;
  }

  // Returns the end index of the first sentence in the given segment of text.
  // If there are no sentences, returns the length of the input text.
  private getSentenceEnd(inputText: string): number {
    try {
      const segments = Array.from(this.sentenceSegmenter_.segment(inputText));
      if (segments.length > 1) {
        // The starting index of the second sentence, i.e. the end index of the
        // first sentence.
        return segments[1]!.index;
      }
      return inputText.length;
    } catch (e) {
      // Intl.Segmenter may throw an error for invalid locales.
      return inputText.length;
    }
  }

  static getInstance(): TextSegmenter {
    return instance || (instance = new TextSegmenter());
  }

  static setInstance(obj: TextSegmenter) {
    instance = obj;
  }
}

let instance: TextSegmenter|null = null;
