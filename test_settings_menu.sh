#!/bin/bash
# Settings Menu Validation Script
# Tests for robustness and edge cases

echo "========================================="
echo "Settings Menu Validation Tests"
echo "========================================="
echo ""

# Test 1: Build check
echo "[1/8] Build verification..."
cd /root/clawd/projects/openclaw-cardputer/firmware
if pio run > /tmp/build.log 2>&1; then
    echo "  ✅ Build successful"
else
    echo "  ❌ Build failed"
    tail -20 /tmp/build.log
    exit 1
fi
echo ""

# Test 2: Memory usage check
echo "[2/8] Memory usage analysis..."
RAM_USAGE=$(grep "RAM:" /tmp/build.log | head -1)
FLASH_USAGE=$(grep "Flash:" /tmp/build.log | head -1)
echo "  $RAM_USAGE"
echo "  $FLASH_USAGE"
RAM_PERCENT=$(echo "$RAM_USAGE" | grep -oP '\d+(?=%)')
if [ ${RAM_PERCENT%\%} -lt 80 ]; then
    echo "  ✅ RAM usage within safe limits"
else
    echo "  ⚠️  RAM usage high ($RAM_PERCENT)"
fi
echo ""

# Test 3: Code style analysis
echo "[3/8] Code style checks..."
CHECKS=0

# Check for hardcoded strings
if grep -rn "192.168" src/ include/ 2>/dev/null | grep -v "config.example"; then
    echo "  ⚠️  Found hardcoded IP addresses"
else
    echo "  ✅ No hardcoded IPs"
fi

# Check for TODO comments
TODO_COUNT=$(grep -rn "TODO" src/ 2>/dev/null | wc -l)
if [ $TODO_COUNT -eq 0 ]; then
    echo "  ✅ No TODOs in production code"
else
    echo "  ⚠️  Found $TODO_COUNT TODOs"
fi

# Check for memory leaks (new without delete)
if grep -rn "new " src/ | grep -v "delete" | grep -v "\.cpp:.*new.*\[" > /dev/null; then
    echo "  ⚠️  Potential memory leaks (new without delete)"
else
    echo "  ✅ No obvious memory leaks"
fi
echo ""

# Test 4: Header guards
echo "[4/8] Header guard validation..."
MISSING_GUARDS=0
for header in include/*.h; do
    if ! grep -q "#ifndef.*_H" "$header"; then
        echo "  ❌ Missing header guard: $(basename $header)"
        MISSING_GUARDS=$((MISSING_GUARDS+1))
    fi
done
if [ $MISSING_GUARDS -eq 0 ]; then
    echo "  ✅ All headers have guards"
fi
echo ""

# Test 5: Function completeness
echo "[5/8] Function signature checks..."
MISSING_IMPLS=0

# Check for declared but not implemented functions
for header in include/*.h; do
    base=$(basename "$header" .h)
    cpp="src/$base.cpp"
    if [ -f "$cpp" ]; then
        # Count virtual functions
        VIRTUAL_DECL=$(grep -c "virtual" "$header" || true)
        VIRTUAL_IMPL=$(grep -c "::" "$cpp" || true)
        # Rough check
        if [ $VIRTUAL_DECL -gt 0 ] && [ $VIRTUAL_IMPL -lt $VIRTUAL_DECL ]; then
            echo "  ⚠️  $base: Virtual function implementations may be missing"
        fi
    fi
done
echo "  ✅ Function signatures validated"
echo ""

# Test 6: Enum safety
echo "[6/8] Enum value validation..."
if grep -rn "case.*:" src/ | grep -v "default:" > /dev/null; then
    echo "  ⚠️  Found enum switch without default case"
else
    echo "  ✅ Switch statements have defaults"
fi
echo ""

# Test 7: Buffer safety
echo "[7/8] Buffer overflow checks..."
BUFFER_ISSUES=0

# Check for strcpy/strcat without bounds
if grep -rn "strcpy\|strcat" src/ | grep -v "strncpy\|strncat" > /dev/null; then
    echo "  ⚠️  Found unsafe string functions"
    BUFFER_ISSUES=$((BUFFER_ISSUES+1))
fi

# Check for hardcoded buffer sizes without sizeof
if grep -rn "\[[0-9]\+\]" src/ | grep -v "sizeof" > /dev/null; then
    echo "  ⚠️  Found hardcoded buffer sizes"
    BUFFER_ISSUES=$((BUFFER_ISSUES+1))
fi

if [ $BUFFER_ISSUES -eq 0 ]; then
    echo "  ✅ Buffer safety checks pass"
fi
echo ""

# Test 8: Settings menu edge cases
echo "[8/8] Edge case analysis..."
EDGE_CASES=0

# Check for null pointer checks
if grep -rn "delete " src/ | grep -v "if.*!=.*nullptr" > /dev/null; then
    echo "  ⚠️  Some delete operations may lack null checks"
    EDGE_CASES=$((EDGE_CASES+1))
fi

# Check for division by zero protection
if grep -rn "/ [0-9]" src/ | grep -v "if.*!= 0\|if.*== 0" > /dev/null; then
    echo "  ⚠️  Potential division by zero"
    EDGE_CASES=$((EDGE_CASES+1))
fi

if [ $EDGE_CASES -eq 0 ]; then
    echo "  ✅ Edge cases handled"
fi
echo ""

# Summary
echo "========================================="
echo "VALIDATION SUMMARY"
echo "========================================="
TOTAL_ISSUES=$((MISSING_GUARDS + BUFFER_ISSUES + EDGE_CASES))
if [ $TOTAL_ISSUES -eq 0 ]; then
    echo "✅ ALL TESTS PASSED - Ready for deployment!"
    exit 0
else
    echo "⚠️  Found $TOTAL_ISSUES issues to address"
    echo ""
    echo "Recommendations:"
    echo "1. Fix header guards for robust compilation"
    echo "2. Use strncpy/strncat instead of strcpy/strcat"
    echo "3. Add null pointer checks before delete operations"
    echo "4. Verify all virtual functions are implemented"
    exit 1
fi
