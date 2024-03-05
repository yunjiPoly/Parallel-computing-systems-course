#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
#include "heatsim.h"
#include "log.h"

struct buffer_t
{
    unsigned int width;
    unsigned int height;
    unsigned int padding;
};

// static MPI_Datatype buffer_type;
// static MPI_Datatype data_buffer_type;


MPI_Datatype create_buffer_type() {
    int err;
    MPI_Datatype buffer_type;
    MPI_Aint displacements[3] = {offsetof(struct buffer_t, width), offsetof(struct buffer_t, height), offsetof(struct buffer_t, padding)};
    MPI_Datatype types[3] = {MPI_UNSIGNED, MPI_UNSIGNED, MPI_UNSIGNED};
    int length[] = {1, 1, 1};
    err = MPI_Type_create_struct(3, length, displacements, types, &buffer_type);
    if (err != MPI_SUCCESS) {
		printf("error MPI_TYPE_CREATE_STRUCT buffer_type");
    }

    MPI_Type_commit(&buffer_type);
    return buffer_type;
}

MPI_Datatype create_data_type(int size) {
    int err;
    MPI_Datatype data_buffer_type;
    MPI_Aint displacements[1] = {0};
    MPI_Datatype types[1] = {MPI_DOUBLE};
    int size_temp[] = {size};
    err = MPI_Type_create_struct(1, size_temp, displacements, types, &data_buffer_type);
    if (err != MPI_SUCCESS) {
		printf("error MPI_TYPE_CREATE_STRUCT data_buffer_type");
    }

    MPI_Type_commit(&data_buffer_type);
    return data_buffer_type;
}


int heatsim_init(heatsim_t* heatsim, unsigned int dim_x, unsigned int dim_y) {
    /*
     * TODO: Initialiser tous les membres de la structure `heatsim`.
     *       Le communicateur doit être périodique. Le communicateur
     *       cartésien est périodique en X et Y.
     */
    
    int err;
    err = MPI_Comm_rank(MPI_COMM_WORLD, &heatsim->rank);
    if (err != MPI_SUCCESS) {
		printf("error MPI_Comm_rank");
        goto fail_exit; 
    }
    err = MPI_Comm_size(MPI_COMM_WORLD, &heatsim->rank_count);
    if (err != MPI_SUCCESS) {
		printf("error MPI_Comm_size");
        goto fail_exit; 
    }
    int dim[2] = {dim_x,dim_y};
    int periodics[2] = {1, 1};
    
    err = MPI_Cart_create(MPI_COMM_WORLD, 2, dim, periodics, false, &heatsim->communicator);
    if (err != MPI_SUCCESS) {
		printf("error MPI_Cart_create");
        goto fail_exit; 
    }
    err = MPI_Cart_shift(heatsim->communicator, 0, 1, &heatsim->rank_west_peer, &heatsim->rank_east_peer);
    if (err != MPI_SUCCESS) {
		printf("error MPI_CART_SHIFT west to east ");
        goto fail_exit; 
    }
    err = MPI_Cart_shift(heatsim->communicator, 1, 1, &heatsim->rank_south_peer, &heatsim->rank_north_peer);
    if (err != MPI_SUCCESS) {
		printf("error MPI_CART_SHIFT south to north");
        goto fail_exit; 
    }
    err = MPI_Cart_coords(heatsim->communicator, heatsim->rank, 2, heatsim->coordinates);
    if (err != MPI_SUCCESS) {
		printf("error MPI_Cart_coords");
        goto fail_exit; 
    }

    

    return 0;
fail_exit:
    return -1;
}

int heatsim_send_grids(heatsim_t* heatsim, cart2d_t* cart) {
    /*
     * TODO: Envoyer toutes les `grid` aux autres rangs. Cette fonction
     *       est appelé pour le rang 0. Par exemple, si le rang 3 est à la
     *       coordonnée cartésienne (0, 2), alors on y envoit le `grid`
     *       à la position (0, 2) dans `cart`.
     *
     *       Il est recommandé d'envoyer les paramètres `width`, `height`
     *       et `padding` avant les données. De cette manière, le receveur
     *       peut allouer la structure avec `grid_create` directement.
     *
     *       Utilisez `cart2d_get_grid` pour obtenir la `grid` à une coordonnée.
     */
    int err;
    for(int dest = 1; dest < heatsim->rank_count; dest++) {
        int coordinates[2];
        err = MPI_Cart_coords(heatsim->communicator, dest, 2, coordinates);
        if (err != MPI_SUCCESS) {
		    printf("error MPI_Cart_coords");
            goto fail_exit; 
        }
        grid_t* grid = cart2d_get_grid(cart, coordinates[0], coordinates[1]);

        struct buffer_t buffer;
        buffer.width = grid->width;
        buffer.height = grid->height;
        buffer.padding = grid->padding;
        
        MPI_Request request;

        MPI_Datatype buffer_type;

        buffer_type = create_buffer_type();

        err = MPI_Isend(&buffer, 1, buffer_type, dest, 0, heatsim->communicator, &request);
        MPI_Wait(&request, MPI_STATUS_IGNORE);
        if (err != MPI_SUCCESS) {
		    printf("error MPI_ISEND buffer");
            goto fail_exit; 
        }
        
        MPI_Datatype data_buffer_type;

        data_buffer_type = create_data_type(grid->width*grid->height);

        err = MPI_Isend(grid->data, 1, data_buffer_type, dest, 0 , heatsim->communicator,&request);
        MPI_Wait(&request, MPI_STATUS_IGNORE);
        if (err != MPI_SUCCESS) {
		    printf("error MPI_ISEND grid_buffer");
            goto fail_exit; 
        }
    }
    return 0;
fail_exit:
    return -1;
}

grid_t* heatsim_receive_grid(heatsim_t* heatsim) {
    /*
     * TODO: Recevoir un `grid ` du rang 0. Il est important de noté que
     *       toutes les `grid` ne sont pas nécessairement de la même
     *       dimension (habituellement ±1 en largeur et hauteur). Utilisez
     *       la fonction `grid_create` pour allouer un `grid`.
     *
     *       Utilisez `grid_create` pour allouer le `grid` à retourner.
     */
    int err;

    MPI_Request request;

    struct buffer_t buffer;

    MPI_Datatype buffer_type;

    buffer_type = create_buffer_type();

    err = MPI_Irecv(&buffer, 1, buffer_type, 0, 0, heatsim->communicator, &request);
    MPI_Wait(&request, MPI_STATUS_IGNORE);
    if (err != MPI_SUCCESS) {
		    printf("error MPI_IRECV buffer");
            return NULL;
    }

    grid_t* grid = grid_create(buffer.width, buffer.height, buffer.padding);

    MPI_Datatype data_buffer_type;

    data_buffer_type = create_data_type(grid->width*grid->height);

    err = MPI_Irecv(grid->data, 1, data_buffer_type, 0, 0, heatsim->communicator, &request);
    MPI_Wait(&request, MPI_STATUS_IGNORE);
    if (err != MPI_SUCCESS) {
		    printf("error MPI_IRECV grid_buffer");
            goto fail_exit; 
    }

    return grid;

fail_exit:
    return NULL;
}

int heatsim_exchange_borders(heatsim_t* heatsim, grid_t* grid) {
    assert(grid->padding == 1);

    /*
     * TODO: Échange les bordures de `grid`, excluant le rembourrage, dans le
     *       rembourrage du voisin de ce rang. Par exemple, soit la `grid`
     *       4x4 suivante,
     *
     *                            +-------------+
     *                            | x x x x x x |
     *                            | x A B C D x |
     *                            | x E F G H x |
     *                            | x I J K L x |
     *                            | x M N O P x |
     *                            | x x x x x x |
     *                            +-------------+
     *
     *       où `x` est le rembourrage (padding = 1). Ce rang devrait envoyer
     *
     *        - la bordure [A B C D] au rang nord,
     *        - la bordure [M N O P] au rang sud,
     *        - la bordure [A E I M] au rang ouest et
     *        - la bordure [D H L P] au rang est.
     *
     *       Ce rang devrait aussi recevoir dans son rembourrage
     *
     *        - la bordure [A B C D] du rang sud,
     *        - la bordure [M N O P] du rang nord,
     *        - la bordure [A E I M] du rang est et
     *        - la bordure [D H L P] du rang ouest.
     *
     *       Après l'échange, le `grid` devrait avoir ces données dans son
     *       rembourrage provenant des voisins:
     *
     *                            +-------------+
     *                            | x m n o p x |
     *                            | d A B C D a |
     *                            | h E F G H e |
     *                            | l I J K L i |
     *                            | p M N O P m |
     *                            | x a b c d x |
     *                            +-------------+
     *
     *       Utilisez `grid_get_cell` pour obtenir un pointeur vers une cellule.
     */
    int err;
    MPI_Request request[8];
    MPI_Status status[8];
    //Send

    //Contiguous
    MPI_Datatype south_north_type;

    err = MPI_Type_contiguous(grid->width, MPI_DOUBLE, &south_north_type);
    if (err != MPI_SUCCESS) {
		printf("error MPI_Type_contiguous south_north_type");
        goto fail_exit; 
    }

    MPI_Type_commit(&south_north_type);

    //North
    err = MPI_Isend(grid_get_cell(grid, 0, grid->height-1), 1, south_north_type, heatsim->rank_north_peer, 1, heatsim->communicator, &request[0]);
    if (err != MPI_SUCCESS) {
		printf("error MPI_ISEND north");
        goto fail_exit; 
    }

    //South
    err = MPI_Isend(grid_get_cell(grid, 0, 0), 1, south_north_type, heatsim->rank_south_peer, 0, heatsim->communicator, &request[1]);
    if (err != MPI_SUCCESS) {
		printf("error MPI_ISEND south");
        goto fail_exit; 
    }

    //Vector
    MPI_Datatype east_west_type;

    err = MPI_Type_vector(grid->height, 1, grid->width_padded,MPI_DOUBLE, &east_west_type);
    if (err != MPI_SUCCESS) {
		printf("error MPI_Type_vector east_west_type");
        goto fail_exit; 
    }

    MPI_Type_commit(&east_west_type);

    //West
    err = MPI_Isend(grid_get_cell(grid, 0, 0), 1, east_west_type, heatsim->rank_west_peer, 0, heatsim->communicator, &request[2]);
    if (err != MPI_SUCCESS) {
		printf("error MPI_ISEND west");
        goto fail_exit; 
    }


    //East
    err = MPI_Isend(grid_get_cell(grid, grid->width-1, 0), 1, east_west_type, heatsim->rank_east_peer, 1, heatsim->communicator, &request[3]);
    if (err != MPI_SUCCESS) {
		printf("error MPI_ISEND east");
        goto fail_exit; 
    }

    //Receive

    //North
    err = MPI_Irecv(grid_get_cell(grid, 0, grid->height), 1, south_north_type, heatsim->rank_north_peer, 0, heatsim->communicator, &request[4]);
    if (err != MPI_SUCCESS) {
		printf("error MPI_ISEND north");
        goto fail_exit; 
    }


    //South
    err = MPI_Irecv(grid_get_cell(grid, 0, -1), 1, south_north_type, heatsim->rank_south_peer, 1, heatsim->communicator, &request[5]);
    if (err != MPI_SUCCESS) {
		printf("error MPI_Irecv south");
        goto fail_exit; 
    }

    //West
    err = MPI_Irecv(grid_get_cell(grid, -1, 0), 1, east_west_type, heatsim->rank_west_peer, 1, heatsim->communicator, &request[6]);
    if (err != MPI_SUCCESS) {
		printf("error MPI_Irecv west");
        goto fail_exit; 
    }


    //East
    err = MPI_Irecv(grid_get_cell(grid, grid->width, 0), 1, east_west_type, heatsim->rank_east_peer, 0, heatsim->communicator, &request[7]);
    if (err != MPI_SUCCESS) {
		printf("error MPI_Irecv east");
        goto fail_exit; 
    }

    err = MPI_Waitall(8, request, status);
    if (err != MPI_SUCCESS) {
		printf("error MPI_Waitall");
        goto fail_exit; 
    }
    return 0;

fail_exit:
    return -1;
}

int heatsim_send_result(heatsim_t* heatsim, grid_t* grid) {
    assert(grid->padding == 0);
    int err;
    /*
     * TODO: Envoyer les données (`data`) du `grid` résultant au rang 0. Le
     *       `grid` n'a aucun rembourage (padding = 0);
     */

    MPI_Datatype data_buffer_type;

    data_buffer_type = create_data_type(grid->width*grid->height);


    err = MPI_Send(grid->data, 1, data_buffer_type, 0, 0, heatsim->communicator);
    if (err != MPI_SUCCESS) {
		printf("error MPI_ISEND buffer");
        goto fail_exit; 
    }
    return 0;

fail_exit:
    return -1;
}

int heatsim_receive_results(heatsim_t* heatsim, cart2d_t* cart) {
    /*
     * TODO: Recevoir toutes les `grid` des autres rangs. Aucune `grid`
     *       n'a de rembourage (padding = 0).
     *
     *       Utilisez `cart2d_get_grid` pour obtenir la `grid` à une coordonnée
     *       qui va recevoir le contenue (`data`) d'un autre noeud.
     */

    int err;
    for(int dest = 1; dest < heatsim->rank_count; dest++) {
        int coordinates[2];
        err = MPI_Cart_coords(heatsim->communicator, dest, 2, coordinates);
        if (err != MPI_SUCCESS) {
		    printf("error MPI_CART_COORDS");
            goto fail_exit; 
        }
        grid_t* grid = cart2d_get_grid(cart, coordinates[0], coordinates[1]);
        MPI_Datatype data_buffer_type;

        data_buffer_type = create_data_type(grid->width*grid->height);

        err = MPI_Recv(grid->data, 1, data_buffer_type, dest, 0 , heatsim->communicator, MPI_STATUS_IGNORE);
        if (err != MPI_SUCCESS) {
		    printf("error MPI_Recv data_buffer_type");
            goto fail_exit; 
        }
    }
    return 0;

fail_exit:
    return -1;
}
