#pragma once

#include_next <linux/pci.h>

#undef DECLARE_PCI_FIXUP_SECTION

#define DECLARE_PCI_FIXUP_SECTION(section, name, vendor, device, class, \
                  class_shift, hook)            \
void __pci_fixup_##hook(struct pci_dev *pdev) { hook(pdev); }
