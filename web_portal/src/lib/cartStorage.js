/**
 * Cart persisted in localStorage under medrover_cart.
 * Search results only load one query at a time — merges visible cart rows
 * into previously saved lines so multi-search flows keep all medicines.
 */

export function readCartLines() {
  try {
    const raw = window.localStorage.getItem('medrover_cart')
    if (!raw) return []
    const parsed = JSON.parse(raw)
    return Array.isArray(parsed) ? parsed : []
  } catch {
    return []
  }
}

/** Lines shaped like SearchPage cart payload */
export function mergeSearchItemsIntoStoredCart(items) {
  const linesFromScreen = items
    .filter((m) => m.inCart)
    .map((m) => ({
      id: m.id,
      name: m.name,
      qtyInfo: m.qty,
      image: m.image,
      price: m.price,
      selectedQty: m.cartQty || 1
    }))
  const prev = readCartLines()
  const touchedIds = new Set(linesFromScreen.map((l) => l.id))
  return [...prev.filter((line) => !touchedIds.has(line.id)), ...linesFromScreen]
}

export function persistCartLines(lines) {
  window.localStorage.setItem('medrover_cart', JSON.stringify(lines))
  window.dispatchEvent(new CustomEvent('medrover_cart_changed'))
}

export function clearCart() {
  window.localStorage.removeItem('medrover_cart')
  window.dispatchEvent(new CustomEvent('medrover_cart_changed'))
}

export function cartTotalQty(lines) {
  return lines.reduce((sum, l) => sum + (l.selectedQty || 1), 0)
}
