/*

  Copyright [2025] [Leonardo Julca]

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

#ifndef AURA_RATE_LIMITER_H
#define AURA_RATE_LIMITER_H

#include "includes.h"

struct TokenBucketRateLimiter
{
  int64_t m_LastRefillTicks;
  int64_t m_TickInterval;
  double m_RefillRate;
  double m_CurrentCapacity;
  double m_MaxCapacity;

  TokenBucketRateLimiter(int64_t tickInterval, double rate, double currentCapacity, double maxCapacity);
  ~TokenBucketRateLimiter();

  [[nodiscard]] inline double GetMaxCapacity() { return m_MaxCapacity; }
  [[nodiscard]] inline double GetCurrentCapacity() { return m_CurrentCapacity; }
  inline void ConsumeWithDebt(double count) { m_CurrentCapacity -= count; }

  inline bool TryConsume(double count) {
    if (m_CurrentCapacity < count) return false;
    m_CurrentCapacity -= count;
    return true;
  }

  inline void Refill(int64_t nowTicks) {
    int64_t tokens = (nowTicks - m_LastRefillTicks) / m_TickInterval;
    if (tokens == 0) return;
    m_CurrentCapacity += static_cast<double>(tokens) * m_RefillRate;
    if (m_MaxCapacity < m_CurrentCapacity) m_CurrentCapacity = m_MaxCapacity;
    m_LastRefillTicks = nowTicks;
  }

  inline void PauseRefillUntil(int64_t futureTicks) {
    if (futureTicks <= m_LastRefillTicks) return;
    m_LastRefillTicks = futureTicks;
  }

  inline bool TryConsume() { return TryConsume(1.); }
  inline void ConsumeWithDebt() { ConsumeWithDebt(1.); }

  inline void SingleRefill() {
    m_CurrentCapacity += m_RefillRate;
    if (m_MaxCapacity < m_CurrentCapacity) m_CurrentCapacity = m_MaxCapacity;
  }

  inline void FullRefill() { m_CurrentCapacity = m_MaxCapacity; }

  inline size_t GetTokensPerMinute() { return static_cast<size_t>(round(m_RefillRate * (60000. / (double)m_TickInterval))); }
};

#endif // AURA_RATE_LIMITER_H
