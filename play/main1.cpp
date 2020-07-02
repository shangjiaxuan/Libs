#include <iostream>
#include <queue>

//The precision of ieee double precision
//float cannot go beyond this
#define LAYER_NUM 12

struct node {
	node* left{};
	node* right{};
	bool leaf = true;
	double P_left{};
	double P_right{};
	int layer = 0;
	int left_step{};
	int right_step{};
};

void init_node(node* cur) {
	if (cur->layer != LAYER_NUM) {
		if (cur->left_step - cur->right_step < 1 && cur->right_step - cur->left_step < 2) {
			cur->leaf = false;
			cur->left = new node();
			cur->left->left_step = cur->left_step + 1;
			cur->left->right_step = cur->right_step;
			cur->left->layer = cur->layer + 1;
			init_node(cur->left);
			cur->right = new node();
			cur->right->left_step = cur->left_step;
			cur->right->right_step = cur->right_step + 1;
			cur->right->layer = cur->layer + 1;
			init_node(cur->right);
			cur->P_right = cur->left->P_right * 0.5 + cur->right->P_right * 0.5;
			cur->P_left = cur->left->P_left * 0.5 + cur->right->P_left * 0.5;
		}
		else if(cur->left_step - cur->right_step >= 1) {
			cur->P_left = 1.0;
		}
		else if (cur->right_step - cur->left_step >= 2) {
			cur->P_right = 1.0;
		}
	}
	else {
		//compensating here may be better, but precision
		//does not require so much
		cur->P_left = 0.5;
		cur->P_right = 0.5;
	}
}

#include <dshow.h>

int main() {
	node* head = new node();
	node* cur = head;
	VFW_E_UNSUPPORTED_STREAM;
	init_node(head);
	printf("left:%f\tright:%f\n",head->P_left,head->P_right);
	return 0;
}