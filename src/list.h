/*

  Copyright [2024] [Leonardo Julca]

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */

#ifndef AURA_LIST_H_
#define AURA_LIST_H_

template <typename T>
struct CircleDoubleLinkedList
{
  T* head;
  T* tail;

  CircleDoubleLinkedList()
   : head(nullptr),
     tail(nullptr)
  {}

  ~CircleDoubleLinkedList() = default;

  inline bool getEmpty() const
  {
    return head == nullptr;
  }

  void insertBefore(T* next, T* node)
  {
    T* prev = next->prev;
    next->prev = node;
    prev->next = node;
    node->next = next;
    node->prev = prev;
  }

  void insertAfter(T* prev, T* node)
  {
    T* next = prev->next;
    prev->next = node;
    next->prev = node;
    node->prev = prev;
    node->next = next;

    if (prev == tail) {
      tail = node;
    }
  }

  void emplaceBefore(T* next)
  {
    insertBefore(next, new T());
  }

  void emplaceAfter(T* prev)
  {
    insertAfter(prev, new T());
  }

  void insertBack(T* node)
  {
    if (getEmpty()) {
      node->next = node;
      node->prev = node;
      head = node;
      tail = node;
      return;
    }
    tail->next = node;
    head->prev = node;
    node->prev = tail;
    node->next = head;
    tail = node;
  }

  void emplaceBack()
  {
    insertBack(new T());
  }

  void remove(T* node)
  {
    T* prev = node->prev;
    T* next = node->next;
    prev->next = next;
    next->prev = prev;

    if (node == tail) {
      tail = prev;
    }
    if (node == head) {
      head = nullptr;
    }
  }
};

#endif // AURA_LIST_H
