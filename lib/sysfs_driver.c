/*
 * sysfs_driver.c
 *
 * Driver utility functions for libsysfs
 *
 * Copyright (C) IBM Corp. 2003
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */
#include "libsysfs.h"
#include "sysfs.h"

void sysfs_close_drv_dev(void *device)
{
	sysfs_close_device((struct sysfs_device *)device);
}

/**
 * sysfs_close_driver: closes and cleans up driver structure
 * NOTE: This routine does not deallocate devices list
 * @driver: driver to close
 */
void sysfs_close_driver(struct sysfs_driver *driver)
{
	if (driver != NULL) {
		if (driver->devices != NULL) {
			dlist_for_each(driver->devices) 
				dlist_shift(driver->devices);
			dlist_destroy(driver->devices);
		}
		if (driver->directory != NULL)
			sysfs_close_directory(driver->directory);
		free(driver);
	}
}

/** 
 * sysfs_close_drv: closes driver and deletes device lists too
 * @driver: driver to close
 */ 
void sysfs_close_drv(struct sysfs_driver *driver)
{
	if (driver != NULL) {
		if (driver->devices != NULL) 
			dlist_destroy(driver->devices);
		if (driver->directory != NULL)
			sysfs_close_directory(driver->directory);
		free(driver);
	}
}
		
	

/**
 * alloc_driver: allocates and initializes driver
 * returns struct sysfs_driver with success and NULL with error.
 */
static struct sysfs_driver *alloc_driver(void)
{
	return (struct sysfs_driver *)calloc(1, sizeof(struct sysfs_driver));
}

/**
 * sysfs_open_driver: opens and initializes driver structure
 * @path: path to driver directory
 * returns struct sysfs_driver with success and NULL with error
 */
struct sysfs_driver *sysfs_open_driver(const unsigned char *path)
{
	struct sysfs_driver *driver = NULL;
	struct sysfs_directory *sdir = NULL;

	if (path == NULL) {
		errno = EINVAL;
		return NULL;
	}
	sdir = sysfs_open_directory(path);
	if (sdir == NULL) {
		dprintf("Error opening directory %s\n", path);
		return NULL;
	}
	if ((sysfs_read_directory(sdir)) != 0) {
		dprintf("Error reading directory %s\n", path);
		sysfs_close_directory(sdir);
		return NULL;
	}
	driver = alloc_driver();
	if (driver == NULL) {
		dprintf("Error allocating driver at %s\n", path);
		sysfs_close_directory(sdir);
		return NULL;
	}
	strcpy(driver->name, sdir->name);
	driver->directory = sdir;	
	strcpy(driver->path, sdir->path);
	
	return driver;
}

/**
 * sysfs_get_driver_attributes: gets list of attributes for the given driver
 * @driver: sysfs_driver for which attributes are required
 * returns a dlist of attributes corresponding to the driver if present
 * 	NULL otherwise
 */
struct dlist *sysfs_get_driver_attributes(struct sysfs_driver *driver)
{
	if (driver == NULL || driver->directory == NULL)
		return NULL;

	return(driver->directory->attributes);
}

/**
 * sysfs_get_driver_links: gets list of links from the given driver
 * @driver: sysfs_driver for which links list is required
 * returns a dlist of links corresponding to the driver if present
 * 	NULL otherwise
 */
struct dlist *sysfs_get_driver_links(struct sysfs_driver *driver)
{
	if (driver == NULL || driver->directory == NULL)
		return NULL;

	return(driver->directory->links);
}

/**
 * sysfs_open_driver_by_name: open a driver by name and return the bus
 * the driver is on.
 * @drv_name: driver to open
 * @bus: the driver bus
 * @bsize: size of bus buffer
 * NOTE: Need to call sysfs_close_drv to free up memory
 * returns struct sysfs_driver if found, NULL otherwise
 */
struct sysfs_driver *sysfs_open_driver_by_name(unsigned char *drv_name,
					unsigned char *bus, size_t bsize)
{
	struct dlist *buslist = NULL, *drivers = NULL, *devices = NULL;
	struct sysfs_driver *driver = NULL;
	struct sysfs_device *device = NULL;
	struct sysfs_link *curlink = NULL;
	char path[SYSFS_PATH_MAX], fullpath[SYSFS_PATH_MAX], 
				*curbus = NULL, *curdrv = NULL;

	if (drv_name == NULL || bus == NULL) {
		errno = EINVAL;
		return NULL;
	}

	memset(path, 0, SYSFS_PATH_MAX);
	memset(fullpath, 0, SYSFS_PATH_MAX);
	buslist = sysfs_open_subsystem_list(SYSFS_BUS_DIR);
	if (buslist != NULL) {
		dlist_for_each_data(buslist, curbus, char) {
			strcpy(path, SYSFS_BUS_DIR);
			strcat(path, "/");
			strcat(path, curbus);
			strcat(path, "/drivers/");
			drivers = sysfs_open_subsystem_list(path);
			if (drivers != NULL) {
				dlist_for_each_data(drivers, curdrv, char) {
					if (strcmp(drv_name, curdrv) == 0) {
						strncpy(bus, curbus, bsize);
						strcat(path, curdrv);
						sysfs_close_list(drivers);
						sysfs_close_list(buslist);
						goto open_device;
					}
				}
				sysfs_close_list(drivers);
			}
		}
		sysfs_close_list(buslist);
		return NULL;
	}

open_device:
	if (sysfs_get_mnt_path(fullpath, SYSFS_PATH_MAX) != 0) {
		dprintf("Error getting sysfs mount path\n");
		return NULL;
	}
	strcat(fullpath, path);
	driver = sysfs_open_driver(fullpath);
	if (driver == NULL) {
		dprintf("Driver %s not found\n", drv_name);
		return NULL;
	}
	if (driver->directory->links != NULL) {
		dlist_for_each_data(driver->directory->links, curlink, 
							struct sysfs_link) {
			device = sysfs_open_device(curlink->target);
			if (device == NULL) {
				dprintf("Error opening device at %s\n", 
						curlink->target);
				sysfs_close_drv(driver);
				return NULL;
			}
			strcpy(device->driver_name, drv_name);
			if (driver->devices == NULL) 
				driver->devices = dlist_new_with_delete
						(sizeof(struct sysfs_device),
					 		sysfs_close_drv_dev);
			dlist_unshift(driver->devices, device);
		}
	}
	return driver;
}

