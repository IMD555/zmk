---
title: Selector Behavior
sidebar_label: Selector
---

## Summary

The Selelector can cycle through the selections depending on the operation.

- SEL_NEXT switches to the next option and performs the corresponding key operation.
- SEL_PREV switches to the previous option and performs the corresponding key operation.
- If separate-pushed is declared, a different operation can be performed with SEL_PUSH.

### Behavior Binding

Example:

```dts
sel0: selector_0 {
    compatible = "zmk,behavior-selector";
    #binding-cells = <1>;
    bindings = 
    <&kp LC(LA(N4))>,       <&sel0 2>,  
    <&kp LC(LA(N5))>,       <&sel0 2>,  
    <&kp LC(LA(N6))>,       <&sel0 1>,  
    <&kp LC(LA(N7))>,       <&sel0 1>,  
    <&kp LC(LA(N8))>,       <&sel0 1>,  
    <&kp LC(LA(N9))>,       <&sel0 1>,  
    <&kp LC(LA(N0))>,       <&sel0 1>,  
    <&kp LC(LA(MINUS))>,    <&sel0 1>,  
    <&kp LC(LA(EQUAL))>,    <&sel0 1>,  
    <&kp LC(LA(GRAV))>,     <&sel0 1>;  
    selection-default = <3>;
    separate-pushed;
};
```

### Configuration

```dts
sel_abc_def: selector_abc_def {
    compatible = "zmk,behavior-selector";
    #binding-cells = <1>;

    bindings = 
    <&kp A>,   <&kp D>,
    <&kp B>,   <&kp E>,
    <&kp C>,   <&kp F>;

    loop;
    separate-pushed;
};
sel_123: selector_123 {
    compatible = "zmk,behavior-selector";
    #binding-cells = <1>;

    bindings = <&kp N1>, <&kp N2>, <&kp N3>;

    selection-default = <1>;
    separate-pushed;
};
```
