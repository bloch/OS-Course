#include "os.h"

// Operating System Homework #1
// By: Nathan Bloch, ID: 316130707

// A side function that calculates the index of the pte at each level of the page-table.
uint64_t get_index(uint64_t vpn, int i) {
	return (vpn >> (36 - 9*i)) & 0x1ff;
}

uint64_t page_table_query(uint64_t pt, uint64_t vpn) {
	uint64_t cur_frame = pt << 12;						// pt is the physical page NUMBER.
	uint64_t pte; uint64_t* p;
	for(int i = 0; i < 5; i++) {					// A page walk, if some pte is not valid - returns NO_MAPPING.
		p = (uint64_t*) phys_to_virt(cur_frame);
		pte = p[get_index(vpn, i)];
		if(pte % 2 == 0)
			return NO_MAPPING;
		cur_frame = pte - 1;
	}
	return cur_frame >> 12;						//Returns value is the ppn -> we need to bits 12-63(including).
}

void page_table_update(uint64_t pt, uint64_t vpn, uint64_t ppn) {
	uint64_t cur_frame, pte, new_frame_num; uint64_t* p;
	uint64_t query;
	if(ppn == NO_MAPPING) {
		query = page_table_query(pt, vpn);
		if(query != NO_MAPPING) {			//IF query == NO_MAPPING, THERE IS NOTHING TO DELTE
			cur_frame = pt << 12;
			for(int i = 0; i < 4; i++) {					// A LOOP TO GET THE LAST PAGE-TABLE.	
				p = (uint64_t*) phys_to_virt(cur_frame);
				pte = p[get_index(vpn, i)];
				cur_frame = pte - 1;
			}

			p = (uint64_t*) phys_to_virt(cur_frame);			// Virtual Pointer to the last page-table(=last frame).
			p[get_index(vpn, 4)] -= 1;							// MAKE IT INVALID BY MAKING VALID BIT=0.
		}
	}
	else {
		cur_frame = pt << 12;
		for(int i = 0; i < 4; i++) {				// Page-walk till the last page table, while creating missing page-tables.
			p = (uint64_t*) phys_to_virt(cur_frame);					// Get Virtual pointer to the frame.
			pte = p[get_index(vpn, i)];									// Get pte using part i of vpn.
			if(pte % 2 == 0) {											
				new_frame_num = alloc_page_frame();
				p[get_index(vpn, i)] = (new_frame_num << 12) + 1;
				cur_frame = new_frame_num << 12;
			}
			else {
				cur_frame = pte - 1;
			}
		}
		// Last page table: here we place the ppn frame provided as a parameter.
		p = (uint64_t*) phys_to_virt(cur_frame);
		// if valid and if not - put new ppn with a valid sign.
		p[get_index(vpn, 4)] = (ppn << 12) + 1;
	}
}