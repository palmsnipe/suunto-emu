# Graphics and display path

## GPU

Think Silicon's public NEMA|p announcement describes a configurable 2D
graphics engine with AMBA interfaces, DMA controllers, and command lists. It
does not publish the Sapporo's integrated register map or command semantics.

- [Think Silicon NEMA|p public announcement](https://www.think-silicon.com/managed_images/News/Think-Silicon_PR_NEMA_p_-FINALE.pdf)
- [NEMA source/evidence reference](../../../../suunto-firmware/docs/research/native-nema-completion-and-diap4-frame.md)

The Sapporo evidence pins module ID `0x86362000`, bus base `0x40090000`, IRQ
28, a circular command ring, and a bounded set of first-frame list operations.
Those observed operations are the emulator contract. Unobserved NEMA registers,
shader behavior, and exact IP revision remain unsupported.

## Physical panel

The Sapporo display path reaches Apollo4 DIAP4 and MSPI1 (`0x40061000`, IRQ21),
and the renderer can publish a surface after a successful NEMA render. The
physical panel controller, wire byte order, and full panel completion are not
identified. This is tracked as `E-SAP-PANEL-001` and `E-NEMA-PANEL-001`.

Ulsan separately observes a NemaDC/display identity register value
`0x87452365` and a 466x466 panel self-test. That is a useful second-family
boundary, not evidence that Ulsan and Sapporo share a panel.
