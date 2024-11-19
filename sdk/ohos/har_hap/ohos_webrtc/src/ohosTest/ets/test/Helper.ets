/**
 * Copyright (c) 2024 Archermind Technology (Nanjing) Co. Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 
import { expect } from '@ohos/hypium';

export type ValueType = number | string | boolean | bigint | object | null | undefined;

export function assertOptionalBoolean(value: ValueType) {
  if(value !== undefined) {
    assertBoolean(value);
  }
}

export function assertOptionalNumber(value: ValueType) {
  if(value !== undefined) {
    assertNumber(value);
  }
}

export function assertOptionalInt(value: ValueType) {
  if(value !== undefined) {
    assertInt(value);
  }
}

export function assertOptionalUnsignedInt(value: ValueType)
{
  if(value !== undefined) {
    assertUnsignedInt(value);
  }
}

export function assertOptionalString(value: ValueType) {
  if(value !== undefined) {
    assertString(value);
  }
}

export function assertBoolean(value: ValueType) {
  expect(typeof value).assertEqual('boolean');
}

function assertNumber(value: ValueType) {
  expect(typeof value).assertEqual('number');
}

function assertInt(value: ValueType) {
  expect(Number.isInteger(value)).assertTrue();
}

export function assertUnsignedInt(value: ValueType) {
  expect(Number.isInteger(value) && (value! >= 0)).assertTrue();
}

export function assertString(value: ValueType) {
  expect(typeof value).assertEqual('string');
}

export function assertArray(value: ValueType) {
  expect(Array.isArray(value)).assertTrue();
}

export function assertObject(value: ValueType) {
  expect(typeof value).assertEqual('object');
  expect(value).not().assertNull();
}

export function sleep(ms: number): Promise<void> {
  return new Promise(resolve => setTimeout(resolve, ms));
}
