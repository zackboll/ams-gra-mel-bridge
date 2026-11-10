//! One allocator for unit-test fault injection across independently owned modules.
//! Controls are thread-local and disabled by default, so parallel tests do not
//! consume each other's failures. No allocator override exists in production.

use std::{
    alloc::{GlobalAlloc, Layout, System},
    cell::Cell,
    ptr,
};

thread_local! {
    pub(crate) static ALLOCATIONS_UNTIL_FAILURE: Cell<isize> = const { Cell::new(-1) };
    pub(crate) static FAIL_SIZE: Cell<usize> = const { Cell::new(0) };
}

fn fail_allocation(size: usize) -> bool {
    let fail_size = FAIL_SIZE
        .try_with(|selected| {
            if selected.get() == size {
                selected.set(0);
                true
            } else {
                false
            }
        })
        .unwrap_or(false);
    let fail_count = ALLOCATIONS_UNTIL_FAILURE
        .try_with(|remaining| {
            let count = remaining.get();
            if count < 0 {
                return false;
            }
            remaining.set(count - 1);
            count == 0
        })
        .unwrap_or(false);
    fail_size || fail_count
}

struct TestAllocator;

// SAFETY: every successful allocation and all deallocations use System with
// unchanged arguments. Explicit thread-local failures return null and disarm
// before error construction, as permitted by GlobalAlloc's contract.
unsafe impl GlobalAlloc for TestAllocator {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        if fail_allocation(layout.size()) {
            ptr::null_mut()
        } else {
            // SAFETY: exact caller-provided layout.
            unsafe { System.alloc(layout) }
        }
    }

    unsafe fn dealloc(&self, pointer: *mut u8, layout: Layout) {
        // SAFETY: all successful allocations originate from System.
        unsafe { System.dealloc(pointer, layout) }
    }

    unsafe fn realloc(&self, pointer: *mut u8, layout: Layout, size: usize) -> *mut u8 {
        if fail_allocation(size) {
            ptr::null_mut()
        } else {
            // SAFETY: exact System allocation and caller-provided layout/size.
            unsafe { System.realloc(pointer, layout, size) }
        }
    }
}

#[global_allocator]
static TEST_ALLOCATOR: TestAllocator = TestAllocator;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn size_failure_is_selective_and_one_shot() {
        FAIL_SIZE.with(|selected| selected.set(17));
        let results = [
            fail_allocation(16),
            fail_allocation(17),
            fail_allocation(17),
        ];
        assert_eq!(results, [false, true, false]);
    }

    #[test]
    fn countdown_failure_is_one_shot() {
        ALLOCATIONS_UNTIL_FAILURE.with(|remaining| remaining.set(1));
        let results = [
            fail_allocation(16),
            fail_allocation(16),
            fail_allocation(16),
        ];
        assert_eq!(results, [false, true, false]);
    }

    #[test]
    fn failure_controls_are_thread_local() {
        std::thread::spawn(|| {
            FAIL_SIZE.with(|selected| selected.set(17));
            ALLOCATIONS_UNTIL_FAILURE.with(|remaining| remaining.set(0));
            let failed = fail_allocation(17);
            let disarmed = !fail_allocation(17);
            assert!(failed && disarmed);
        })
        .join()
        .unwrap();
        assert!(!fail_allocation(17));
    }
}
